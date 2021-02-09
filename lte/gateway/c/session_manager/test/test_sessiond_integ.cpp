/**
 * Copyright 2020 The Magma Authors.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <string.h>
#include <chrono>
#include <thread>
#include <future>

#include <grpc++/grpc++.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Consts.h"
#include "SessionReporter.h"
#include "MagmaService.h"
#include "Matchers.h"
#include "ProtobufCreators.h"
#include "ServiceRegistrySingleton.h"
#include "SessionManagerServer.h"
#include "SessiondMocks.h"
#include "SessionStore.h"
#include "LocalEnforcer.h"

#define SESSION_TERMINATION_TIMEOUT_MS 100
#define DEFAULT_PIPELINED_EPOCH 1

using grpc::Status;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::Test;

namespace magma {

class SessiondTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    auto test_channel = ServiceRegistrySingleton::Instance()->GetGrpcChannel(
        "test_service", ServiceRegistrySingleton::LOCAL);
    evb = new folly::EventBase();

    controller_mock = std::make_shared<MockCentralController>();
    pipelined_mock  = std::make_shared<MockPipelined>();

    pipelined_client  = std::make_shared<AsyncPipelinedClient>(test_channel);
    directoryd_client = std::make_shared<AsyncDirectorydClient>(test_channel);
    spgw_client       = std::make_shared<AsyncSpgwServiceClient>(test_channel);
    events_reporter   = std::make_shared<MockEventsReporter>();
    auto rule_store   = std::make_shared<StaticRuleStore>();
    session_store     = std::make_shared<SessionStore>(rule_store);
    insert_static_rule(rule_store, 1, "rule1");
    insert_static_rule(rule_store, 1, "rule2");
    insert_static_rule(rule_store, 2, "rule3");

    reporter = std::make_shared<SessionReporterImpl>(evb, test_channel);
    auto default_mconfig = get_default_mconfig();
    enforcer             = std::make_shared<LocalEnforcer>(
        reporter, rule_store, *session_store, pipelined_client,
        directoryd_client, events_reporter, spgw_client, nullptr,
        SESSION_TERMINATION_TIMEOUT_MS, 0, default_mconfig);
    session_map = SessionMap{};

    local_service =
        std::make_shared<service303::MagmaService>("sessiond", "1.0");
    session_manager = std::make_shared<LocalSessionManagerAsyncService>(
        local_service->GetNewCompletionQueue(),
        std::make_unique<LocalSessionManagerHandlerImpl>(
            enforcer, reporter.get(), directoryd_client, events_reporter,
            *session_store));

    proxy_responder = std::make_shared<SessionProxyResponderAsyncService>(
        local_service->GetNewCompletionQueue(),
        std::make_unique<SessionProxyResponderHandlerImpl>(
            enforcer, *session_store));

    local_service->AddServiceToServer(session_manager.get());
    local_service->AddServiceToServer(proxy_responder.get());

    test_service =
        std::make_shared<service303::MagmaService>("test_service", "1.0");
    test_service->AddServiceToServer(controller_mock.get());
    test_service->AddServiceToServer(pipelined_mock.get());

    local_service->Start();

    std::thread([&]() {
      std::cout << "Started test_service thread\n";
      test_service->Start();
      test_service->WaitForShutdown();
    })
        .detach();
    std::thread([&]() {
      std::cout << "Started pipelined client response thread\n";
      pipelined_client->rpc_response_loop();
    })
        .detach();
    std::thread([&]() { spgw_client->rpc_response_loop(); }).detach();
    std::thread([&]() {
      std::cout << "Started main event base thread\n";
      folly::EventBaseManager::get()->setEventBase(evb, 0);
      enforcer->attachEventBase(evb);
      enforcer->start();
    })
        .detach();
    std::thread([&]() {
      std::cout << "Started reporter thread\n";
      reporter->rpc_response_loop();
    })
        .detach();
    std::thread([&]() {
      std::cout << "Started local grpc thread\n";
      session_manager->wait_for_requests();
    })
        .detach();
    std::thread([&]() {
      std::cout << "Started local grpc thread\n";
      proxy_responder->wait_for_requests();
    })
        .detach();
    evb->waitUntilRunning();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  virtual void TearDown() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    local_service->Stop();
    enforcer->stop();
    reporter->stop();
    test_service->Stop();
    pipelined_client->stop();
    // This is a failsafe in case callbacks keep running on the event loop
    // longer than intended for the unit test
    evb->terminateLoopSoon();
    bool has_exited = !evb->isRunning();
    for (int i = 0; i < 10; i++) {
      if (has_exited) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
      has_exited = !evb->isRunning();
    }
    if (!has_exited) {
      std::cout << "EventBase eventloop is still running and should not be. "
                << "You might see a segfault as everything scheduled to run "
                << "is not guaranteed to have access to the things they should."
                << std::endl;
      EXPECT_TRUE(false);
    }
  }

  void insert_static_rule(
      std::shared_ptr<StaticRuleStore> rule_store, uint32_t charging_key,
      const std::string& rule_id) {
    auto mkey = "";
    PolicyRule rule;
    create_policy_rule(rule_id, mkey, charging_key, &rule);
    rule_store->insert_rule(rule);
  }

  // Timeout to not block test
  void set_timeout(uint32_t ms, std::promise<void>* call_promise) {
    std::thread([&]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(ms));
      EXPECT_TRUE(false);
      call_promise->set_value();
    })
        .detach();
  }

  // This function should always be called at the beginning of the test to
  // prevent unexpected SessionD <-> PipelineD syncing logic mid-test.
  void send_empty_pipelined_table(
      std::unique_ptr<LocalSessionManager::Stub>& stub) {
    RuleRecordTable empty_table;
    // epoch indicates the last PipelineD service start time
    empty_table.set_epoch(DEFAULT_PIPELINED_EPOCH);
    grpc::ClientContext context;
    Void void_resp;
    stub->ReportRuleStats(&context, empty_table, &void_resp);
  }

  void send_update_pipelined_table(
      std::unique_ptr<LocalSessionManager::Stub>& stub, RuleRecordTable table) {
    grpc::ClientContext context;
    Void void_resp;
    // The epoch should be consistent with the initial empty table to prevent
    // SessionD from attempting to sync information.
    table.set_epoch(DEFAULT_PIPELINED_EPOCH);
    stub->ReportRuleStats(&context, table, &void_resp);
  }

 protected:
  folly::EventBase* evb;
  std::shared_ptr<MockCentralController> controller_mock;
  std::shared_ptr<MockPipelined> pipelined_mock;
  std::shared_ptr<LocalEnforcer> enforcer;
  std::shared_ptr<SessionReporterImpl> reporter;
  std::shared_ptr<LocalSessionManagerAsyncService> session_manager;
  std::shared_ptr<SessionProxyResponderAsyncService> proxy_responder;
  std::shared_ptr<service303::MagmaService> local_service;
  std::shared_ptr<service303::MagmaService> test_service;
  std::shared_ptr<AsyncPipelinedClient> pipelined_client;
  std::shared_ptr<AsyncDirectorydClient> directoryd_client;
  std::shared_ptr<AsyncSpgwServiceClient> spgw_client;
  std::shared_ptr<MockEventsReporter> events_reporter;
  std::shared_ptr<SessionStore> session_store;
  SessionMap session_map;
};

ACTION_P2(SetEndPromise, promise_p, status) {
  promise_p->set_value();
  return status;
}

ACTION_P(SetPromise, promise_p) {
  promise_p->set_value();
}

// Take the SessionID from the request as it is generated internally
ACTION_P2(SetCreateSessionResponse, first_quota, second_quota) {
  auto req  = static_cast<const CreateSessionRequest*>(arg1);
  auto res  = static_cast<CreateSessionResponse*>(arg2);
  auto imsi = req->common_context().sid().id();
  res->mutable_static_rules()->Add()->mutable_rule_id()->assign("rule1");
  res->mutable_static_rules()->Add()->mutable_rule_id()->assign("rule2");
  res->mutable_static_rules()->Add()->mutable_rule_id()->assign("rule3");
  create_credit_update_response(
      imsi, req->session_id(), 1, first_quota, res->mutable_credits()->Add());
  create_credit_update_response(
      imsi, req->session_id(), 2, second_quota, res->mutable_credits()->Add());
}
// Take the SessionID from the request as it is generated internally
ACTION_P(SetUpdateSessionResponse, quota) {
  auto req        = static_cast<const UpdateSessionRequest*>(arg1)->updates(0);
  auto res        = static_cast<UpdateSessionResponse*>(arg2);
  auto imsi       = req.common_context().sid().id();
  auto session_id = req.session_id();
  create_credit_update_response(
      imsi, session_id, 1, quota, res->mutable_responses()->Add());
}

ACTION(SetSessionTerminateResponse) {
  auto req = static_cast<const SessionTerminateRequest*>(arg1);
  auto res = static_cast<SessionTerminateResponse*>(arg2);
  res->set_sid(req->common_context().sid().id());
  res->set_session_id(req->session_id());
}



TEST_F(SessiondTest, many_ues) {
  std::promise<void> create_promise, tunnel_promise, update_promise,
      terminate_promise;
  std::promise<std::string> session_id_promise;
  std::string ipv4_addrs  = "192.168.0.1";
  std::string ipv6_addrs  = "2001:0db8:85a3:0000:0000:8a2e:0370:7334";
  uint32_t default_bearer = 5;
  //uint32_t enb_teid       = TEID_1_DL;
  //uint32_t agw_teid       = TEID_1_UL;

  auto channel = ServiceRegistrySingleton::Instance()->GetGrpcChannel(
      "sessiond", ServiceRegistrySingleton::LOCAL);
  auto stub = LocalSessionManager::NewStub(channel);

  // Setup the SessionD/PipelineD epoch value by sending an empty record. This
  // will prevent SessionD thinking that PipelineD has restarted mid-test.
  send_empty_pipelined_table(stub);

  EXPECT_CALL(
      *controller_mock,
      CreateSession(
          testing::_, testing::_,
          testing::_))
      .Times(2)
      .WillOnce(testing::DoAll(
          SetCreateSessionResponse(1536, 1024),
          testing::Return(grpc::Status::OK)));




  // send_empty_pipelined_table(stub);
  for(int i= 0; i< 2; i++) {
    // 1- CreateSession Trigger
    grpc::ClientContext create_context;
    LocalCreateSessionResponse create_resp;
    LocalCreateSessionRequest request;

    auto imsi = IMSI1;
    if (i == 1) {
        imsi = IMSI2;
      }

    request.mutable_common_context()->mutable_sid()->set_id(imsi);
    request.mutable_common_context()->set_rat_type(RATType::TGPP_LTE);
    request.mutable_rat_specific_context()
        ->mutable_lte_context()
        ->set_bearer_id(default_bearer);
    request.mutable_common_context()->set_ue_ipv4(ipv4_addrs);
    request.mutable_common_context()->set_ue_ipv6(ipv6_addrs);
    // Todo, remove emptyTeids once we split CreateSession
    Teids emptyTeids;
    emptyTeids.set_enb_teid(0);
    emptyTeids.set_agw_teid(0);
    request.mutable_common_context()->mutable_teids()->CopyFrom(emptyTeids);

    stub->CreateSession(&create_context, request, &create_resp);
  }
  // Block and wait until we process CreateSessionResponse, after which the
  // SessionID value will be set.
  std::string session_id = session_id_promise.get_future().get();
  // The thread needs to be halted before proceeding to call ReportRuleStats()
  // because the call to PipelineD within CreateSession() is an async call,
  // and the call to PipelineD, ActivateFlows(), is assumed in this test
  // to happened before the ReportRuleStats().
  create_promise.get_future().get();


}






/**
 * End to end test.
 * 1) Create session, respond with 2 charging keys
 * 2) Report rule stats, charging key 1 goes over
 *    Expect update with charging key 1
 * 3) End Session for IMSI1
 * 4) Report rule stats without stats for IMSI1 (terminated)
 *    Expect update with terminated charging keys 1 and 2
 * One thing to note is that, even though the main thread is halted twice
 * to enforce the order of function calls to demo a simple case
 * creating session --> updating usage --> terminating session,
 * in reality, the order of those functions is not guaranteed
 * because of the following two reasons.
 * 1) All function calls to either feg or PipelineD are async calls.
 * 2) ReportRuleStats() is an GRPC invoked by PipelineD periodically no matter
 *    if we have any alive session or not. The invoking of ReportRuleStats()
 *    is completely independent of both CreateSession() and EndSession().
 */
TEST_F(SessiondTest, end_to_end_success) {
  std::promise<void> create_promise, tunnel_promise, update_promise,
      terminate_promise;
  std::promise<std::string> session_id_promise;
  std::string ipv4_addrs  = "192.168.0.1";
  std::string ipv6_addrs  = "2001:0db8:85a3:0000:0000:8a2e:0370:7334";
  uint32_t default_bearer = 5;
  uint32_t enb_teid       = TEID_1_DL;
  uint32_t agw_teid       = TEID_1_UL;

  auto channel = ServiceRegistrySingleton::Instance()->GetGrpcChannel(
      "sessiond", ServiceRegistrySingleton::LOCAL);
  auto stub = LocalSessionManager::NewStub(channel);

  // Setup the SessionD/PipelineD epoch value by sending an empty record. This
  // will prevent SessionD thinking that PipelineD has restarted mid-test.
  send_empty_pipelined_table(stub);

  {
    // 1- CreateSession Expectations
    // Expect create session with IMSI1

    EXPECT_CALL(
        *controller_mock,
        CreateSession(
            testing::_, CheckCreateSession(IMSI1, &session_id_promise),
            testing::_))
        .Times(1)
        .WillOnce(testing::DoAll(
            SetCreateSessionResponse(1536, 1024), SetPromise(&create_promise),
            testing::Return(grpc::Status::OK)));
  }

  // send_empty_pipelined_table(stub);

  // 1- CreateSession Trigger
  grpc::ClientContext create_context;
  LocalCreateSessionResponse create_resp;
  LocalCreateSessionRequest request;
  request.mutable_common_context()->mutable_sid()->set_id(IMSI1);
  request.mutable_common_context()->set_rat_type(RATType::TGPP_LTE);
  request.mutable_rat_specific_context()->mutable_lte_context()->set_bearer_id(
      default_bearer);
  request.mutable_common_context()->set_ue_ipv4(ipv4_addrs);
  request.mutable_common_context()->set_ue_ipv6(ipv6_addrs);
  // Todo, remove emptyTeids once we split CreateSession
  Teids emptyTeids;
  emptyTeids.set_enb_teid(0);
  emptyTeids.set_agw_teid(0);
  request.mutable_common_context()->mutable_teids()->CopyFrom(emptyTeids);

  stub->CreateSession(&create_context, request, &create_resp);

  // Block and wait until we process CreateSessionResponse, after which the
  // SessionID value will be set.
  std::string session_id = session_id_promise.get_future().get();
  // The thread needs to be halted before proceeding to call ReportRuleStats()
  // because the call to PipelineD within CreateSession() is an async call,
  // and the call to PipelineD, ActivateFlows(), is assumed in this test
  // to happened before the ReportRuleStats().
  create_promise.get_future().get();

  {
    // 2- UpdateTunnelIds Expectations
    // Expect rules to be installed and pipelined to be configured
    EXPECT_CALL(
        *events_reporter,
        session_created(IMSI1, testing::_, testing::_, testing::_))
        .Times(1);

    // Temporary fix for PipelineD client in SessionD introduces separate
    // calls for static and dynamic rules. So here is the call for static
    // rules.
    EXPECT_CALL(
        *pipelined_mock,
        ActivateFlows(
            testing::_,
            CheckActivateFlowsForTunnIds(
                IMSI1, ipv4_addrs, ipv6_addrs, enb_teid, agw_teid, 3),
            testing::_))
        .Times(1);
    // Here is the call for dynamic rules, which in this case should be empty.
    EXPECT_CALL(
        *pipelined_mock,
        ActivateFlows(
            testing::_,
            CheckActivateFlowsForTunnIds(
                IMSI1, ipv4_addrs, ipv6_addrs, enb_teid, agw_teid, 0),
            testing::_))
        .WillOnce(testing::DoAll(
            SetPromise(&tunnel_promise), testing::Return(grpc::Status::OK)));
  }

  // 2- UpdateTunnelIds Trigger
  grpc::ClientContext tun_update_context;
  UpdateTunnelIdsResponse tun_update_response;
  UpdateTunnelIdsRequest tun_update_request;
  tun_update_request.mutable_sid()->set_id(IMSI1);
  tun_update_request.set_bearer_id(default_bearer);
  tun_update_request.set_enb_teid(enb_teid);
  tun_update_request.set_agw_teid(agw_teid);
  stub->UpdateTunnelIds(
      &tun_update_context, tun_update_request, &tun_update_response);

  // Wait until the ActivateFlows call from UpdateTunnelIds has completed
  tunnel_promise.get_future().get();

  {
    InSequence s;
    // 3 - PipelineD update + UpdateSession Expectations
    CreditUsageUpdate expected_update;
    create_usage_update(
        IMSI1, 1, 1024, 512, CreditUsage::QUOTA_EXHAUSTED, &expected_update);

    // Expect update with IMSI1, charging key 1
    EXPECT_CALL(
        *controller_mock,
        UpdateSession(
            testing::_, CheckSingleUpdate(expected_update), testing::_))
        .Times(1)
        .WillOnce(testing::DoAll(
            SetUpdateSessionResponse(1024), SetPromise(&update_promise),
            testing::Return(grpc::Status::OK)));

    EXPECT_CALL(
        *events_reporter, session_updated(session_id, testing::_, testing::_))
        .Times(1);
  }

  // 3- ReportRuleStats Trigger
  RuleRecordTable table;
  auto record_list = table.mutable_records();
  create_rule_record(IMSI1, ipv4_addrs, "rule1", 512, 512, record_list->Add());
  create_rule_record(IMSI1, ipv6_addrs, "rule2", 512, 0, record_list->Add());
  create_rule_record(IMSI1, ipv4_addrs, "rule3", 32, 32, record_list->Add());
  send_update_pipelined_table(stub, table);

  // The thread needs to be halted before proceeding to call EndSession()
  // because the call to FeG within ReportRuleStats() is an async call,
  // and the call to FeG, UpdateSession(), is assumed in this test
  // to happened before the EndSession().
  // Wait until the ActivateFlows call from UpdateTunnelIds has completed
  update_promise.get_future().get();

  {
    // 4- EndSession Expectations
    // Expect flows to be deactivated before final update is sent out
    EXPECT_CALL(
        *pipelined_mock,
        DeactivateFlows(testing::_, CheckDeactivateFlows(IMSI1), testing::_))
        .Times(1);

    EXPECT_CALL(*events_reporter, session_terminated(IMSI1, testing::_))
        .Times(1);

    EXPECT_CALL(
        *controller_mock,
        TerminateSession(testing::_, CheckTerminate(IMSI1), testing::_))
        .Times(1)
        .WillOnce(testing::DoAll(
            SetSessionTerminateResponse(), SetPromise(&terminate_promise),
            testing::Return(grpc::Status::OK)));
  }

  // 4- EndSession Trigger
  LocalEndSessionResponse end_resp;
  grpc::ClientContext end_context;
  LocalEndSessionRequest end_request;
  end_request.mutable_sid()->set_id(IMSI1);
  stub->EndSession(&end_context, end_request, &end_resp);
  // The current logic for termination is that we wait until all rules for the
  // session has finished reporting usage from PipelineD.
  send_empty_pipelined_table(stub);

  set_timeout(5000, &terminate_promise);
  terminate_promise.get_future().get();
}

/**
 * End to end test with cloud service intermittent.
 * 1) Create session, respond with 2 charging keys
 * 2) Report rule stats, charging key 1 goes over
 *    Expect update with charging key 1
 * 3) Cloud will respond with a timeout
 * 4) Report rule stats for charging key 1 again
 *    Since the last update failed, this will trigger another update
 * 5) Expect update with usage from both (2) and (4).
 */
TEST_F(SessiondTest, end_to_end_cloud_down) {
  std::promise<void> end_promise, failed_update_promise;
  std::promise<std::string> session_id_promise;
  uint32_t default_bearer = 5;
  uint32_t enb_teid       = TEID_1_DL;
  uint32_t agw_teid       = TEID_1_UL;
  auto channel = ServiceRegistrySingleton::Instance()->GetGrpcChannel(
      "sessiond", ServiceRegistrySingleton::LOCAL);
  auto stub = LocalSessionManager::NewStub(channel);
  // Setup the SessionD/PipelineD epoch value by sending an empty record. This
  // will prevent SessionD thinking that PipelineD has restarted mid-test.
  send_empty_pipelined_table(stub);

  {
    // Expect create session with IMSI1
    EXPECT_CALL(
        *controller_mock,
        CreateSession(
            testing::_, CheckCreateSession(IMSI1, &session_id_promise),
            testing::_))
        .Times(1)
        .WillOnce(testing::DoAll(
            SetCreateSessionResponse(1025, 1024),
            testing::Return(grpc::Status::OK)));
  }
  // 1- Create session
  grpc::ClientContext create_context;
  LocalCreateSessionResponse create_resp;
  LocalCreateSessionRequest request;
  request.mutable_common_context()->mutable_sid()->set_id(IMSI1);
  request.mutable_common_context()->set_rat_type(RATType::TGPP_LTE);
  request.mutable_rat_specific_context()->mutable_lte_context()->set_bearer_id(
      default_bearer);
  stub->CreateSession(&create_context, request, &create_resp);

  // 2- UpdateTunnelIds Trigger
  grpc::ClientContext tun_update_context;
  UpdateTunnelIdsResponse tun_update_response;
  UpdateTunnelIdsRequest tun_update_request;
  tun_update_request.mutable_sid()->set_id(IMSI1);
  tun_update_request.set_bearer_id(default_bearer);
  tun_update_request.set_enb_teid(enb_teid);
  tun_update_request.set_agw_teid(agw_teid);
  stub->UpdateTunnelIds(
      &tun_update_context, tun_update_request, &tun_update_response);

  {
    CreditUsageUpdate expected_update_fail;
    create_usage_update(
        IMSI1, 1, 512, 512, CreditUsage::QUOTA_EXHAUSTED,
        &expected_update_fail);
    // Expect update with IMSI1, charging key 1, return timeout from cloud
    EXPECT_CALL(
        *controller_mock,
        UpdateSession(
            testing::_, CheckSingleUpdate(expected_update_fail), testing::_))
        .Times(1)
        .WillOnce(
            testing::Return(grpc::Status(grpc::DEADLINE_EXCEEDED, "timeout")));
  }

  RuleRecordTable table1;
  create_rule_record(IMSI1, "rule1", 0, 512, table1.mutable_records()->Add());
  create_rule_record(IMSI1, "rule2", 512, 0, table1.mutable_records()->Add());
  send_update_pipelined_table(stub, table1);

  // Need to wait for cloud response to come back and usage monitor to reset.
  // Unfortunately, there is no simple way to wait for response to come back
  // then callback to be called in event base
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  {
    CreditUsageUpdate expected_update_success;
    create_usage_update(
        IMSI1, 1, 1024, 1024, CreditUsage::QUOTA_EXHAUSTED,
        &expected_update_success);
    // second update should contain the original usage report + new usage
    // report since the first update failed
    EXPECT_CALL(
        *controller_mock,
        UpdateSession(
            testing::_, CheckSingleUpdate(expected_update_success), testing::_))
        .Times(1)
        .WillOnce(SetEndPromise(&end_promise, Status::OK));
  }

  RuleRecordTable table2;
  create_rule_record(IMSI1, "rule1", 512, 0, table2.mutable_records()->Add());
  create_rule_record(IMSI1, "rule2", 0, 512, table2.mutable_records()->Add());
  send_update_pipelined_table(stub, table2);

  set_timeout(5000, &end_promise);
  end_promise.get_future().get();
}

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = 1;
  FLAGS_v           = 10;
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

}  // namespace magma
