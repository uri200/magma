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
#include <future>
#include <memory>
#include <utility>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "ProtobufCreators.h"
#include "SessionState.h"
#include "SessiondMocks.h"
#include "magma_logging.h"

using ::testing::Test;

namespace magma {
const SessionConfig test_sstate_cfg = {.ue_ipv4   = "127.0.0.1",
                                       .spgw_ipv4 = "128.0.0.1"};

class SessionStateTest : public ::testing::Test {
 protected:
 protected:
  virtual void SetUp() {
    auto tgpp_ctx = TgppContext();
    create_tgpp_context("gx.dest.com", "gy.dest.com", &tgpp_ctx);
    rule_store    = std::make_shared<StaticRuleStore>();
    session_state = std::make_shared<SessionState>(
        "imsi", "session", "", test_sstate_cfg, *rule_store, tgpp_ctx);
    update_criteria = get_default_update_criteria();
  }
  enum RuleType {
    STATIC  = 0,
    DYNAMIC = 1,
  };

  void insert_rule(
      uint32_t rating_group, const std::string& m_key,
      const std::string& rule_id, RuleType rule_type,
      std::time_t activation_time, std::time_t deactivation_time) {
    PolicyRule rule;
    create_policy_rule(rule_id, m_key, rating_group, &rule);
    RuleLifetime lifetime{
        .activation_time   = activation_time,
        .deactivation_time = deactivation_time,
    };
    switch (rule_type) {
      case STATIC:
        // insert into list of existing rules
        rule_store->insert_rule(rule);
        // mark the rule as active in session
        session_state->activate_static_rule(rule_id, lifetime, update_criteria);
        break;
      case DYNAMIC:
        session_state->insert_dynamic_rule(rule, lifetime, update_criteria);
        break;
    }
  }

  void schedule_rule(
      uint32_t rating_group, const std::string& m_key,
      const std::string& rule_id, RuleType rule_type,
      std::time_t activation_time, std::time_t deactivation_time) {
    PolicyRule rule;
    create_policy_rule(rule_id, m_key, rating_group, &rule);
    RuleLifetime lifetime{
        .activation_time   = activation_time,
        .deactivation_time = deactivation_time,
    };
    switch (rule_type) {
      case STATIC:
        // insert into list of existing rules
        rule_store->insert_rule(rule);
        // mark the rule as scheduled in the session
        session_state->schedule_static_rule(rule_id, lifetime, update_criteria);
        break;
      case DYNAMIC:
        session_state->schedule_dynamic_rule(rule, lifetime, update_criteria);
        break;
    }
  }

  // TODO: make session_manager.proto and policydb.proto to use common field
  static RedirectInformation_AddressType address_type_converter(
      RedirectServer_RedirectAddressType address_type) {
    switch (address_type) {
      case RedirectServer_RedirectAddressType_IPV4:
        return RedirectInformation_AddressType_IPv4;
      case RedirectServer_RedirectAddressType_IPV6:
        return RedirectInformation_AddressType_IPv6;
      case RedirectServer_RedirectAddressType_URL:
        return RedirectInformation_AddressType_URL;
      case RedirectServer_RedirectAddressType_SIP_URI:
        return RedirectInformation_AddressType_SIP_URI;
      default:
        return RedirectInformation_AddressType_IPv4;
    }
  }

  void insert_gy_redirection_rule(const std::string& rule_id) {
    PolicyRule redirect_rule;
    redirect_rule.set_id(rule_id);
    redirect_rule.set_priority(999);

    RedirectInformation* redirect_info = redirect_rule.mutable_redirect();
    redirect_info->set_support(RedirectInformation_Support_ENABLED);

    RedirectServer redirect_server;
    redirect_server.set_redirect_address_type(RedirectServer::URL);
    redirect_server.set_redirect_server_address("http://www.example.com/");

    redirect_info->set_address_type(
        address_type_converter(redirect_server.redirect_address_type()));
    redirect_info->set_server_address(
        redirect_server.redirect_server_address());

    RuleLifetime lifetime{};
    session_state->insert_gy_dynamic_rule(
        redirect_rule, lifetime, update_criteria);
  }

  void receive_credit_from_ocs(uint32_t rating_group, uint64_t volume) {
    CreditUpdateResponse charge_resp;
    create_credit_update_response("IMSI1", rating_group, volume, &charge_resp);
    session_state->receive_charging_credit(charge_resp, update_criteria);
  }

  void receive_credit_from_ocs(uint32_t rating_group, uint64_t total_volume,
                               uint64_t tx_volume,uint64_t rx_volume, bool is_final) {
    CreditUpdateResponse charge_resp;
    create_credit_update_response("IMSI1", rating_group,total_volume, tx_volume,
                                  rx_volume, is_final, &charge_resp);
    session_state->receive_charging_credit(charge_resp, update_criteria);
  }

  void receive_credit_from_pcrf(
      const std::string& mkey, uint64_t volume, MonitoringLevel level) {
    UsageMonitoringUpdateResponse monitor_resp;
    create_monitor_update_response("IMSI1", mkey, level, volume, &monitor_resp);
    session_state->receive_monitor(monitor_resp, update_criteria);
  }

  void activate_rule(
      uint32_t rating_group, const std::string& m_key,
      const std::string& rule_id, RuleType rule_type,
      std::time_t activation_time, std::time_t deactivation_time) {
    PolicyRule rule;
    create_policy_rule(rule_id, m_key, rating_group, &rule);
    RuleLifetime lifetime{
        .activation_time   = activation_time,
        .deactivation_time = deactivation_time,
    };
    switch (rule_type) {
      case STATIC:
        rule_store->insert_rule(rule);
        session_state->activate_static_rule(rule_id, lifetime, update_criteria);
        break;
      case DYNAMIC:
        session_state->insert_dynamic_rule(rule, lifetime, update_criteria);
        break;
    }
  }

 protected:
  std::shared_ptr<StaticRuleStore> rule_store;
  std::shared_ptr<SessionState> session_state;
  SessionStateUpdateCriteria update_criteria;
};

TEST_F(SessionStateTest, test_session_rules) {
  activate_rule(1, "m1", "rule1", DYNAMIC, 0, 0);
  EXPECT_EQ(1, session_state->total_monitored_rules_count());
  activate_rule(2, "m2", "rule2", STATIC, 0, 0);
  EXPECT_EQ(2, session_state->total_monitored_rules_count());
  // add a OCS-ONLY static rule
  activate_rule(3, "", "rule3", STATIC, 0, 0);
  EXPECT_EQ(2, session_state->total_monitored_rules_count());

  std::vector<std::string> rules_out{};
  std::vector<std::string>& rules_out_ptr = rules_out;

  session_state->get_dynamic_rules().get_rule_ids(rules_out_ptr);
  EXPECT_EQ(rules_out_ptr.size(), 1);
  EXPECT_EQ(rules_out_ptr[0], "rule1");

  EXPECT_EQ(session_state->is_static_rule_installed("rule2"), true);
  EXPECT_EQ(session_state->is_static_rule_installed("rule3"), true);
  EXPECT_EQ(session_state->is_static_rule_installed("rule_DNE"), false);

  // Test rule removals
  PolicyRule rule_out;
  session_state->deactivate_static_rule("rule2", update_criteria);
  EXPECT_EQ(1, session_state->total_monitored_rules_count());
  EXPECT_EQ(
      true,
      session_state->remove_dynamic_rule("rule1", &rule_out, update_criteria));
  EXPECT_EQ("m1", rule_out.monitoring_key());
  EXPECT_EQ(0, session_state->total_monitored_rules_count());

  // basic sanity checks to see it's properly deleted
  rules_out = {};
  session_state->get_dynamic_rules().get_rule_ids(rules_out_ptr);
  EXPECT_EQ(rules_out_ptr.size(), 0);

  rules_out = {};
  session_state->get_dynamic_rules().get_rule_ids_for_monitoring_key(
      "m1", rules_out);
  EXPECT_EQ(0, rules_out.size());

  std::string mkey;
  // searching for non-existent rule should fail
  EXPECT_EQ(
      false, session_state->get_dynamic_rules().get_monitoring_key_for_rule_id(
                 "rule1", &mkey));
  // deleting an already deleted rule should fail
  EXPECT_EQ(
      false,
      session_state->get_dynamic_rules().remove_rule("rule1", &rule_out));
}

/**
 * Check that rule scheduling and installation works from the perspective of
 * tracking in SessionState
 */
TEST_F(SessionStateTest, test_rule_scheduling) {
  auto _uc = get_default_update_criteria();  // unused

  // First schedule a dynamic and static rule. They are treated as inactive.
  schedule_rule(1, "m1", "rule1", DYNAMIC, 0, 0);
  EXPECT_EQ(0, session_state->total_monitored_rules_count());
  EXPECT_FALSE(session_state->is_dynamic_rule_installed("rule1"));

  schedule_rule(2, "m2", "rule2", STATIC, 0, 0);
  EXPECT_EQ(0, session_state->total_monitored_rules_count());
  EXPECT_FALSE(session_state->is_static_rule_installed("rule2"));

  // Now suppose some time has passed, and it's time to mark scheduled rules
  // as active. The responsibility is given to the session owner to make
  // these calls
  session_state->install_scheduled_dynamic_rule("rule1", _uc);
  EXPECT_EQ(1, session_state->total_monitored_rules_count());
  EXPECT_TRUE(session_state->is_dynamic_rule_installed("rule1"));

  session_state->install_scheduled_static_rule("rule2", _uc);
  EXPECT_EQ(2, session_state->total_monitored_rules_count());
  EXPECT_TRUE(session_state->is_static_rule_installed("rule2"));
}

/**
 * Check that on restart, sessions can be updated to match the current time
 */
TEST_F(SessionStateTest, test_rule_time_sync) {
  auto uc = get_default_update_criteria();  // unused

  // These should be active after sync
  schedule_rule(1, "m1", "d1", DYNAMIC, 5, 15);
  schedule_rule(1, "m1", "s1", STATIC, 5, 15);

  // These should still be scheduled
  schedule_rule(1, "m1", "d2", DYNAMIC, 15, 20);
  schedule_rule(1, "m1", "s2", STATIC, 15, 20);

  // These should be expired afterwards
  schedule_rule(2, "m2", "d3", DYNAMIC, 2, 4);
  schedule_rule(2, "m2", "s3", STATIC, 2, 4);

  EXPECT_FALSE(session_state->is_dynamic_rule_installed("d1"));
  EXPECT_FALSE(session_state->is_dynamic_rule_installed("d2"));
  EXPECT_FALSE(session_state->is_dynamic_rule_installed("d3"));

  EXPECT_FALSE(session_state->is_static_rule_installed("s1"));
  EXPECT_FALSE(session_state->is_static_rule_installed("s2"));
  EXPECT_FALSE(session_state->is_static_rule_installed("s3"));

  // Update the time, and sync the rule states, then check our expectations
  std::time_t test_time(10);
  session_state->sync_rules_to_time(test_time, uc);

  EXPECT_TRUE(session_state->is_dynamic_rule_installed("d1"));
  EXPECT_FALSE(session_state->is_dynamic_rule_installed("d2"));
  EXPECT_FALSE(session_state->is_dynamic_rule_installed("d3"));

  EXPECT_TRUE(session_state->is_static_rule_installed("s1"));
  EXPECT_FALSE(session_state->is_static_rule_installed("s2"));
  EXPECT_FALSE(session_state->is_static_rule_installed("s3"));

  EXPECT_EQ(uc.dynamic_rules_to_install.size(), 1);
  EXPECT_EQ(uc.dynamic_rules_to_install.front().id(), "d1");
  EXPECT_TRUE(uc.dynamic_rules_to_uninstall.count("d3"));

  EXPECT_TRUE(uc.static_rules_to_install.count("s1"));
  EXPECT_TRUE(uc.static_rules_to_uninstall.count("s3"));

  // Update the time once more, sync again, and check expectations
  test_time = std::time_t(16);
  uc        = get_default_update_criteria();
  session_state->sync_rules_to_time(test_time, uc);

  EXPECT_FALSE(session_state->is_dynamic_rule_installed("d1"));
  EXPECT_TRUE(session_state->is_dynamic_rule_installed("d2"));
  EXPECT_FALSE(session_state->is_dynamic_rule_installed("d3"));

  EXPECT_FALSE(session_state->is_static_rule_installed("s1"));
  EXPECT_TRUE(session_state->is_static_rule_installed("s2"));
  EXPECT_FALSE(session_state->is_static_rule_installed("s3"));

  EXPECT_EQ(uc.dynamic_rules_to_install.size(), 1);
  EXPECT_EQ(uc.dynamic_rules_to_install.front().id(), "d2");
  EXPECT_TRUE(uc.dynamic_rules_to_uninstall.count("d1"));

  EXPECT_TRUE(uc.static_rules_to_install.count("s2"));
  EXPECT_TRUE(uc.static_rules_to_uninstall.count("s1"));
}

TEST_F(SessionStateTest, test_marshal_unmarshal) {
  EXPECT_EQ(update_criteria.static_rules_to_install.size(), 0);
  insert_rule(1, "m1", "rule1", STATIC, 0, 0);
  EXPECT_EQ(session_state->is_static_rule_installed("rule1"), true);
  EXPECT_EQ(true, session_state->active_monitored_rules_exist());
  EXPECT_EQ(update_criteria.static_rules_to_install.size(), 1);

  std::time_t activation_time =
      static_cast<std::time_t>(std::stoul("2020:04:15 09:10:11"));
  std::time_t deactivation_time =
      static_cast<std::time_t>(std::stoul("2020:04:15 09:10:12"));

  EXPECT_EQ(update_criteria.new_rule_lifetimes.size(), 1);
  schedule_rule(1, "m1", "rule2", DYNAMIC, activation_time, deactivation_time);
  EXPECT_EQ(session_state->is_dynamic_rule_installed("rule2"), false);
  EXPECT_EQ(update_criteria.static_rules_to_install.size(), 1);

  EXPECT_EQ(update_criteria.charging_credit_to_install.size(), 0);
  receive_credit_from_ocs(1, 1024);
  EXPECT_EQ(update_criteria.charging_credit_to_install.size(), 1);
  EXPECT_EQ(session_state->get_charging_credit(1, ALLOWED_TOTAL), 1024);

  EXPECT_EQ(update_criteria.monitor_credit_to_install.size(), 0);
  receive_credit_from_pcrf("m1", 1024, MonitoringLevel::PCC_RULE_LEVEL);
  EXPECT_EQ(session_state->get_monitor("m1", ALLOWED_TOTAL), 1024);
  EXPECT_EQ(update_criteria.monitor_credit_to_install.size(), 1);

  auto marshaled   = session_state->marshal();
  auto unmarshaled = SessionState::unmarshal(marshaled, *rule_store);
  EXPECT_EQ(unmarshaled->get_charging_credit(1, ALLOWED_TOTAL), 1024);
  EXPECT_EQ(unmarshaled->get_monitor("m1", ALLOWED_TOTAL), 1024);
  EXPECT_EQ(unmarshaled->is_static_rule_installed("rule1"), true);
  EXPECT_EQ(session_state->is_dynamic_rule_installed("rule2"), false);
}

TEST_F(SessionStateTest, test_insert_credit) {
  EXPECT_EQ(update_criteria.static_rules_to_install.size(), 0);
  insert_rule(1, "m1", "rule1", STATIC, 0, 0);
  EXPECT_EQ(true, session_state->active_monitored_rules_exist());
  EXPECT_TRUE(
      std::find(
          update_criteria.static_rules_to_install.begin(),
          update_criteria.static_rules_to_install.end(),
          "rule1") != update_criteria.static_rules_to_install.end());

  receive_credit_from_ocs(1, 1024);
  EXPECT_EQ(session_state->get_charging_credit(1, ALLOWED_TOTAL), 1024);
  EXPECT_EQ(
      update_criteria.charging_credit_to_install[CreditKey(1)]
          .credit.buckets[ALLOWED_TOTAL],
      1024);

  receive_credit_from_pcrf("m1", 1024, MonitoringLevel::PCC_RULE_LEVEL);
  EXPECT_EQ(session_state->get_monitor("m1", ALLOWED_TOTAL), 1024);
  EXPECT_EQ(
      update_criteria.monitor_credit_to_install["m1"]
          .credit.buckets[ALLOWED_TOTAL],
      1024);
}

TEST_F(SessionStateTest, test_can_complete_termination) {
  MockSessionReporter reporter;

  insert_rule(1, "m1", "rule1", STATIC, 0, 0);
  EXPECT_EQ(true, session_state->active_monitored_rules_exist());
  EXPECT_TRUE(
      std::find(
          update_criteria.static_rules_to_install.begin(),
          update_criteria.static_rules_to_install.end(),
          "rule1") != update_criteria.static_rules_to_install.end());
  // Have not received credit
  EXPECT_EQ(update_criteria.monitor_credit_map.size(), 0);

  EXPECT_EQ(session_state->can_complete_termination(), false);

  session_state->start_termination(update_criteria);
  EXPECT_EQ(session_state->can_complete_termination(), false);

  // If the rule is still being reported, termination should not be completed.
  auto _uc = get_default_update_criteria();
  session_state->new_report(_uc);
  EXPECT_EQ(session_state->can_complete_termination(), false);
  session_state->add_rule_usage("rule1", 100, 100, update_criteria);
  EXPECT_EQ(session_state->can_complete_termination(), false);
  EXPECT_EQ(update_criteria.monitor_credit_map.size(), 0);
  session_state->finish_report(_uc);
  EXPECT_EQ(session_state->can_complete_termination(), false);

  // The rule is not reported, termination can be completed.
  session_state->new_report(_uc);
  EXPECT_EQ(session_state->can_complete_termination(), false);
  session_state->finish_report(_uc);
  EXPECT_EQ(session_state->can_complete_termination(), true);

  // Termination should only be completed once.
  session_state->complete_termination(reporter, update_criteria);
  EXPECT_EQ(session_state->can_complete_termination(), false);
}

TEST_F(SessionStateTest, test_add_rule_usage) {
  insert_rule(1, "m1", "rule1", STATIC, 0, 0);
  insert_rule(2, "m2", "dyn_rule1", DYNAMIC, 0, 0);
  EXPECT_EQ(true, session_state->active_monitored_rules_exist());
  EXPECT_TRUE(
      std::find(
          update_criteria.static_rules_to_install.begin(),
          update_criteria.static_rules_to_install.end(),
          "rule1") != update_criteria.static_rules_to_install.end());
  EXPECT_EQ(update_criteria.dynamic_rules_to_install.size(), 1);

  receive_credit_from_ocs(1, 3000);
  receive_credit_from_ocs(2, 6000);
  EXPECT_EQ(update_criteria.charging_credit_to_install.size(), 2);
  EXPECT_EQ(
      update_criteria.charging_credit_to_install[CreditKey(1)]
          .credit.buckets[ALLOWED_TOTAL],
      3000);

  receive_credit_from_pcrf("m1", 3000, MonitoringLevel::PCC_RULE_LEVEL);
  receive_credit_from_pcrf("m2", 6000, MonitoringLevel::PCC_RULE_LEVEL);
  EXPECT_EQ(update_criteria.monitor_credit_to_install.size(), 2);
  EXPECT_EQ(
      update_criteria.monitor_credit_to_install["m1"]
          .credit.buckets[ALLOWED_TOTAL],
      3000);

  session_state->add_rule_usage("rule1", 2000, 1000, update_criteria);
  EXPECT_EQ(session_state->get_charging_credit(1, USED_TX), 2000);
  EXPECT_EQ(session_state->get_charging_credit(1, USED_RX), 1000);
  EXPECT_EQ(session_state->get_monitor("m1", USED_TX), 2000);
  EXPECT_EQ(session_state->get_monitor("m1", USED_RX), 1000);
  EXPECT_EQ(
      update_criteria.charging_credit_map[CreditKey(1)].bucket_deltas[USED_TX],
      2000);
  EXPECT_EQ(
      update_criteria.monitor_credit_map["m1"].bucket_deltas[USED_RX], 1000);

  session_state->add_rule_usage("dyn_rule1", 4000, 2000, update_criteria);
  EXPECT_EQ(session_state->get_charging_credit(2, USED_TX), 4000);
  EXPECT_EQ(session_state->get_charging_credit(2, USED_RX), 2000);
  EXPECT_EQ(session_state->get_monitor("m2", USED_TX), 4000);
  EXPECT_EQ(session_state->get_monitor("m2", USED_RX), 2000);
  EXPECT_EQ(
      update_criteria.charging_credit_map[CreditKey(2)].bucket_deltas[USED_TX],
      4000);
  EXPECT_EQ(
      update_criteria.monitor_credit_map["m2"].bucket_deltas[USED_RX], 2000);

  UpdateSessionRequest update;
  std::vector<std::unique_ptr<ServiceAction>> actions;
  session_state->get_updates(update, &actions, update_criteria);
  EXPECT_EQ(actions.size(), 0);
  EXPECT_EQ(update.updates_size(), 2);
  EXPECT_EQ(update.usage_monitors_size(), 2);

  PolicyRule policy_out;
  EXPECT_EQ(
      true, session_state->remove_dynamic_rule(
                "dyn_rule1", &policy_out, update_criteria));
  EXPECT_EQ(
      true, session_state->deactivate_static_rule("rule1", update_criteria));
  EXPECT_EQ(false, session_state->active_monitored_rules_exist());
  EXPECT_TRUE(
      std::find(
          update_criteria.dynamic_rules_to_uninstall.begin(),
          update_criteria.dynamic_rules_to_uninstall.end(),
          "dyn_rule1") != update_criteria.dynamic_rules_to_uninstall.end());
}

TEST_F(SessionStateTest, test_mixed_tracking_rules) {
  insert_rule(0, "m1", "dyn_rule1", DYNAMIC, 0, 0);
  insert_rule(2, "", "dyn_rule2", DYNAMIC, 0, 0);
  insert_rule(3, "m3", "dyn_rule3", DYNAMIC, 0, 0);
  EXPECT_EQ(true, session_state->active_monitored_rules_exist());
  // Installing a rule doesn't install credit
  EXPECT_EQ(update_criteria.charging_credit_to_install.size(), 0);
  EXPECT_EQ(update_criteria.dynamic_rules_to_install.size(), 3);

  receive_credit_from_ocs(2, 6000);
  receive_credit_from_ocs(3, 8000);
  EXPECT_EQ(update_criteria.charging_credit_to_install.size(), 2);
  EXPECT_EQ(
      update_criteria.charging_credit_to_install[CreditKey(2)]
          .credit.buckets[ALLOWED_TOTAL],
      6000);

  receive_credit_from_pcrf("m1", 3000, MonitoringLevel::PCC_RULE_LEVEL);
  receive_credit_from_pcrf("m3", 8000, MonitoringLevel::PCC_RULE_LEVEL);
  EXPECT_EQ(
      update_criteria.monitor_credit_to_install["m1"]
          .credit.buckets[ALLOWED_TOTAL],
      3000);

  session_state->add_rule_usage("dyn_rule1", 2000, 1000, update_criteria);
  EXPECT_EQ(session_state->get_monitor("m1", USED_TX), 2000);
  EXPECT_EQ(session_state->get_monitor("m1", USED_RX), 1000);

  EXPECT_EQ(
      update_criteria.monitor_credit_map["m1"].bucket_deltas[USED_TX], 2000);
  EXPECT_EQ(
      update_criteria.monitor_credit_map["m1"].bucket_deltas[USED_RX], 1000);

  session_state->add_rule_usage("dyn_rule2", 4000, 2000, update_criteria);
  EXPECT_EQ(session_state->get_charging_credit(2, USED_TX), 4000);
  EXPECT_EQ(session_state->get_charging_credit(2, USED_RX), 2000);
  EXPECT_EQ(
      update_criteria.charging_credit_map[CreditKey(2)].bucket_deltas[USED_TX],
      4000);
  session_state->add_rule_usage("dyn_rule3", 5000, 3000, update_criteria);
  EXPECT_EQ(session_state->get_charging_credit(3, USED_TX), 5000);
  EXPECT_EQ(session_state->get_charging_credit(3, USED_RX), 3000);
  EXPECT_EQ(
      update_criteria.charging_credit_map[CreditKey(3)].bucket_deltas[USED_TX],
      5000);
  EXPECT_EQ(session_state->get_monitor("m3", USED_TX), 5000);
  EXPECT_EQ(session_state->get_monitor("m3", USED_RX), 3000);
  EXPECT_EQ(
      update_criteria.monitor_credit_map["m3"].bucket_deltas[USED_TX], 5000);

  UpdateSessionRequest update;
  std::vector<std::unique_ptr<ServiceAction>> actions;
  session_state->get_updates(update, &actions, update_criteria);
  EXPECT_EQ(actions.size(), 0);
  EXPECT_EQ(update.updates_size(), 2);
  EXPECT_EQ(update.usage_monitors_size(), 2);
}

TEST_F(SessionStateTest, test_session_level_key) {
  EXPECT_EQ(nullptr, session_state->get_session_level_key());

  receive_credit_from_pcrf("m1", 8000, MonitoringLevel::SESSION_LEVEL);
  EXPECT_EQ("m1", *session_state->get_session_level_key());
  EXPECT_EQ(session_state->get_monitor("m1", ALLOWED_TOTAL), 8000);
  EXPECT_EQ(
      update_criteria.monitor_credit_to_install["m1"]
          .credit.buckets[ALLOWED_TOTAL],
      8000);

  session_state->add_rule_usage("rule1", 5000, 3000, update_criteria);
  EXPECT_EQ(session_state->get_monitor("m1", USED_TX), 5000);
  EXPECT_EQ(session_state->get_monitor("m1", USED_RX), 3000);

  EXPECT_EQ(
      update_criteria.monitor_credit_map["m1"].bucket_deltas[USED_TX], 5000);
  EXPECT_EQ(
      update_criteria.monitor_credit_map["m1"].bucket_deltas[USED_RX], 3000);

  UpdateSessionRequest update;
  std::vector<std::unique_ptr<ServiceAction>> actions;
  session_state->get_updates(update, &actions, update_criteria);
  EXPECT_EQ(actions.size(), 0);
  EXPECT_EQ(update.usage_monitors_size(), 1);
  auto& single_update = update.usage_monitors(0).update();
  EXPECT_EQ(single_update.level(), MonitoringLevel::SESSION_LEVEL);
  EXPECT_EQ(single_update.bytes_rx(), 3000);
  EXPECT_EQ(single_update.bytes_tx(), 5000);
}

TEST_F(SessionStateTest, test_reauth_key) {
  insert_rule(1, "", "rule1", STATIC, 0, 0);

  receive_credit_from_ocs(1, 1500);

  session_state->add_rule_usage("rule1", 1000, 500, update_criteria);

  UpdateSessionRequest update;
  std::vector<std::unique_ptr<ServiceAction>> actions;
  session_state->get_updates(update, &actions, update_criteria);
  EXPECT_EQ(update.updates_size(), 1);
  EXPECT_EQ(session_state->get_charging_credit(1, REPORTING_TX), 1000);
  EXPECT_EQ(session_state->get_charging_credit(1, REPORTING_RX), 500);
  // Reporting value is not tracked by UpdateCriteria
  EXPECT_EQ(
      update_criteria.charging_credit_map[CreditKey(1)]
          .bucket_deltas[REPORTING_TX],
      0);
  EXPECT_EQ(
      update_criteria.charging_credit_map[CreditKey(1)]
          .bucket_deltas[REPORTING_RX],
      0);
  // credit is already reporting, no update needed
  auto uc         = get_default_update_criteria();
  auto reauth_res = session_state->reauth_key(1, uc);
  EXPECT_EQ(reauth_res, ReAuthResult::UPDATE_NOT_NEEDED);
  receive_credit_from_ocs(1, 1024);
  EXPECT_EQ(session_state->get_charging_credit(1, REPORTING_TX), 0);
  EXPECT_EQ(session_state->get_charging_credit(1, REPORTING_RX), 0);
  reauth_res = session_state->reauth_key(1, uc);
  EXPECT_EQ(reauth_res, ReAuthResult::UPDATE_INITIATED);

  session_state->add_rule_usage("rule1", 2, 1, update_criteria);
  UpdateSessionRequest reauth_update;
  session_state->get_updates(reauth_update, &actions, update_criteria);
  EXPECT_EQ(reauth_update.updates_size(), 1);
  auto& usage = reauth_update.updates(0).usage();
  EXPECT_EQ(usage.bytes_tx(), 2);
  EXPECT_EQ(usage.bytes_rx(), 1);
}

TEST_F(SessionStateTest, test_reauth_new_key) {
  // credit is already reporting, no update needed
  auto reauth_res = session_state->reauth_key(1, update_criteria);
  EXPECT_EQ(reauth_res, ReAuthResult::UPDATE_INITIATED);

  // assert stored charging grant fields are updated to reflect reauth state
  EXPECT_EQ(update_criteria.charging_credit_to_install.size(), 1);
  auto new_charging_credits = update_criteria.charging_credit_to_install;
  EXPECT_EQ(new_charging_credits[1].reauth_state, REAUTH_REQUIRED);

  UpdateSessionRequest reauth_update;
  std::vector<std::unique_ptr<ServiceAction>> actions;
  session_state->get_updates(reauth_update, &actions, update_criteria);
  EXPECT_EQ(reauth_update.updates_size(), 1);
  auto& usage = reauth_update.updates(0).usage();
  EXPECT_EQ(usage.charging_key(), 1);
  EXPECT_EQ(usage.bytes_tx(), 0);
  EXPECT_EQ(usage.bytes_rx(), 0);

  // assert stored charging grant fields are updated to reflect reauth state
  EXPECT_EQ(update_criteria.charging_credit_map.size(), 1);
  auto credit_uc = update_criteria.charging_credit_map[1];
  EXPECT_EQ(credit_uc.reauth_state, REAUTH_PROCESSING);

  receive_credit_from_ocs(1, 1024);
  EXPECT_EQ(session_state->get_charging_credit(1, ALLOWED_TOTAL), 1024);
  EXPECT_EQ(
      update_criteria.charging_credit_map[CreditKey(1)]
          .bucket_deltas[ALLOWED_TOTAL],
      1024);

  // assert stored charging grant fields are updated to reflect reauth state
  EXPECT_EQ(update_criteria.charging_credit_map.size(), 1);
  credit_uc = update_criteria.charging_credit_map[1];
  EXPECT_EQ(credit_uc.reauth_state, REAUTH_NOT_NEEDED);
}

TEST_F(SessionStateTest, test_reauth_all) {
  insert_rule(1, "", "rule1", STATIC, 0, 0);
  insert_rule(2, "", "dyn_rule1", DYNAMIC, 0, 0);
  EXPECT_EQ(false, session_state->active_monitored_rules_exist());
  EXPECT_TRUE(
      std::find(
          update_criteria.static_rules_to_install.begin(),
          update_criteria.static_rules_to_install.end(),
          "rule1") != update_criteria.static_rules_to_install.end());
  EXPECT_EQ(update_criteria.dynamic_rules_to_install.size(), 1);

  receive_credit_from_ocs(1, 1024);
  receive_credit_from_ocs(2, 1024);

  session_state->add_rule_usage("rule1", 10, 20, update_criteria);
  session_state->add_rule_usage("dyn_rule1", 30, 40, update_criteria);
  // If any charging key isn't reporting, an update is needed
  auto uc         = get_default_update_criteria();
  auto reauth_res = session_state->reauth_all(uc);
  EXPECT_EQ(reauth_res, ReAuthResult::UPDATE_INITIATED);

  // assert stored charging grant fields are updated to reflect reauth state
  EXPECT_EQ(uc.charging_credit_map.size(), 2);
  auto credit_uc_1 = uc.charging_credit_map[1];
  auto credit_uc_2 = uc.charging_credit_map[2];
  EXPECT_EQ(credit_uc_1.reauth_state, REAUTH_REQUIRED);
  EXPECT_EQ(credit_uc_2.reauth_state, REAUTH_REQUIRED);

  UpdateSessionRequest reauth_update;
  std::vector<std::unique_ptr<ServiceAction>> actions;
  session_state->get_updates(reauth_update, &actions, uc);
  EXPECT_EQ(reauth_update.updates_size(), 2);

  EXPECT_EQ(uc.charging_credit_map.size(), 2);
  credit_uc_1 = uc.charging_credit_map[1];
  credit_uc_2 = uc.charging_credit_map[2];
  EXPECT_EQ(credit_uc_1.reauth_state, REAUTH_PROCESSING);
  EXPECT_EQ(credit_uc_2.reauth_state, REAUTH_PROCESSING);

  // All charging keys are reporting, no update needed
  reauth_res = session_state->reauth_all(uc);
  EXPECT_EQ(reauth_res, ReAuthResult::UPDATE_NOT_NEEDED);

  EXPECT_EQ(uc.charging_credit_map.size(), 2);
  credit_uc_1 = uc.charging_credit_map[1];
  credit_uc_2 = uc.charging_credit_map[2];
  EXPECT_EQ(credit_uc_1.reauth_state, REAUTH_PROCESSING);
  EXPECT_EQ(credit_uc_2.reauth_state, REAUTH_PROCESSING);
}

TEST_F(SessionStateTest, test_tgpp_context_is_set_on_update) {
  receive_credit_from_pcrf("m1", 1024, MonitoringLevel::PCC_RULE_LEVEL);
  receive_credit_from_ocs(1, 1024);
  insert_rule(1, "m1", "rule1", STATIC, 0, 0);
  session_state->add_rule_usage("rule1", 1024, 0, update_criteria);
  EXPECT_EQ(true, session_state->active_monitored_rules_exist());

  EXPECT_EQ(session_state->get_monitor("m1", ALLOWED_TOTAL), 1024);
  EXPECT_EQ(session_state->get_charging_credit(1, ALLOWED_TOTAL), 1024);
  EXPECT_EQ(
      update_criteria.charging_credit_to_install[CreditKey(1)]
          .credit.buckets[ALLOWED_TOTAL],
      1024);
  EXPECT_EQ(
      update_criteria.monitor_credit_to_install["m1"]
          .credit.buckets[ALLOWED_TOTAL],
      1024);

  UpdateSessionRequest update;
  std::vector<std::unique_ptr<ServiceAction>> actions;
  session_state->get_updates(update, &actions, update_criteria);
  EXPECT_EQ(actions.size(), 0);
  EXPECT_EQ(update.updates_size(), 1);
  EXPECT_EQ(update.updates().Get(0).tgpp_ctx().gx_dest_host(), "gx.dest.com");
  EXPECT_EQ(update.updates().Get(0).tgpp_ctx().gy_dest_host(), "gy.dest.com");
  EXPECT_EQ(update.usage_monitors_size(), 1);
  EXPECT_EQ(
      update.usage_monitors().Get(0).tgpp_ctx().gx_dest_host(), "gx.dest.com");
  EXPECT_EQ(
      update.usage_monitors().Get(0).tgpp_ctx().gy_dest_host(), "gy.dest.com");
}

TEST_F(SessionStateTest, test_get_total_credit_usage_single_rule_no_key) {
  insert_rule(0, "", "rule1", STATIC, 0, 0);
  session_state->add_rule_usage("rule1", 2000, 1000, update_criteria);
  SessionState::TotalCreditUsage actual =
      session_state->get_total_credit_usage();
  EXPECT_EQ(actual.monitoring_tx, 0);
  EXPECT_EQ(actual.monitoring_rx, 0);
  EXPECT_EQ(actual.charging_tx, 0);
  EXPECT_EQ(actual.charging_rx, 0);
}

TEST_F(SessionStateTest, test_get_total_credit_usage_single_rule_single_key) {
  insert_rule(1, "", "rule1", STATIC, 0, 0);
  receive_credit_from_ocs(1, 3000);
  session_state->add_rule_usage("rule1", 2000, 1000, update_criteria);
  SessionState::TotalCreditUsage actual =
      session_state->get_total_credit_usage();
  EXPECT_EQ(actual.monitoring_tx, 0);
  EXPECT_EQ(actual.monitoring_rx, 0);
  EXPECT_EQ(actual.charging_tx, 2000);
  EXPECT_EQ(actual.charging_rx, 1000);
}

TEST_F(SessionStateTest, test_get_total_credit_usage_single_rule_multiple_key) {
  insert_rule(1, "m1", "rule1", STATIC, 0, 0);
  receive_credit_from_ocs(1, 3000);
  receive_credit_from_pcrf("m1", 3000, MonitoringLevel::PCC_RULE_LEVEL);
  session_state->add_rule_usage("rule1", 2000, 1000, update_criteria);
  SessionState::TotalCreditUsage actual =
      session_state->get_total_credit_usage();
  EXPECT_EQ(actual.monitoring_tx, 2000);
  EXPECT_EQ(actual.monitoring_rx, 1000);
  EXPECT_EQ(actual.charging_tx, 2000);
  EXPECT_EQ(actual.charging_rx, 1000);
}

TEST_F(SessionStateTest, test_get_total_credit_usage_multiple_rule_shared_key) {
  // Shared monitoring key
  // One rule is dynamic
  insert_rule(1, "m1", "rule1", STATIC, 0, 0);
  insert_rule(0, "m1", "rule2", DYNAMIC, 0, 0);
  receive_credit_from_ocs(1, 3000);
  receive_credit_from_pcrf("m1", 3000, MonitoringLevel::PCC_RULE_LEVEL);
  session_state->add_rule_usage("rule1", 1000, 10, update_criteria);
  session_state->add_rule_usage("rule2", 500, 5, update_criteria);
  SessionState::TotalCreditUsage actual =
      session_state->get_total_credit_usage();
  EXPECT_EQ(actual.monitoring_tx, 1500);
  EXPECT_EQ(actual.monitoring_rx, 15);
  EXPECT_EQ(actual.charging_tx, 1000);
  EXPECT_EQ(actual.charging_rx, 10);
}

TEST_F(SessionStateTest, test_install_gy_rules) {
  insert_gy_redirection_rule("redirect");

  std::vector<std::string> rules_out{};
  std::vector<std::string>& rules_out_ptr = rules_out;

  session_state->get_gy_dynamic_rules().get_rule_ids(rules_out_ptr);
  EXPECT_EQ(rules_out_ptr.size(), 1);
  EXPECT_EQ(rules_out_ptr[0], "redirect");

  EXPECT_TRUE(session_state->is_gy_dynamic_rule_installed("redirect"));
  EXPECT_EQ(update_criteria.gy_dynamic_rules_to_install.size(), 1);

  PolicyRule rule_out;
  EXPECT_EQ(
      true, session_state->remove_gy_dynamic_rule(
                "redirect", &rule_out, update_criteria));
  // basic sanity checks to see it's properly deleted
  rules_out = {};
  session_state->get_gy_dynamic_rules().get_rule_ids(rules_out_ptr);
  EXPECT_EQ(rules_out_ptr.size(), 0);

  EXPECT_FALSE(session_state->is_gy_dynamic_rule_installed("redirect"));
  EXPECT_EQ(update_criteria.gy_dynamic_rules_to_uninstall.size(), 1);

  rules_out = {};
  session_state->get_gy_dynamic_rules().get_rule_ids_for_monitoring_key(
      "m1", rules_out);
  EXPECT_EQ(0, rules_out.size());

  std::string mkey;
  // searching for non-existent rule should fail
  EXPECT_FALSE(
      session_state->get_dynamic_rules().get_monitoring_key_for_rule_id(
          "redirect", &mkey));
  // deleting an already deleted rule should fail
  EXPECT_FALSE(
      session_state->get_dynamic_rules().remove_rule("redirect", &rule_out));
}

TEST_F(SessionStateTest, test_final_credit_install) {
  insert_rule(1, "m1", "rule1", STATIC, 0, 0);
  CreditUpdateResponse charge_resp;
  charge_resp.set_success(true);
  charge_resp.set_sid("IMSI1");
  charge_resp.set_charging_key(1);

  bool is_final = true;
  auto p_credit = charge_resp.mutable_credit();
  create_charging_credit(1024, is_final, p_credit);
  auto redirect = p_credit->mutable_redirect_server();
  redirect->set_redirect_server_address("google.com");
  redirect->set_redirect_address_type(RedirectServer_RedirectAddressType_URL);
  p_credit->set_final_action(ChargingCredit_FinalAction_REDIRECT);

  session_state->receive_charging_credit(charge_resp, update_criteria);

  // Test that the update criteria is filled out properly
  EXPECT_EQ(update_criteria.charging_credit_to_install.size(), 1);
  auto u_credit = update_criteria.charging_credit_to_install[1];
  EXPECT_TRUE(u_credit.is_final);
  auto fa = u_credit.final_action_info;
  EXPECT_EQ(fa.final_action, ChargingCredit_FinalAction_REDIRECT);
  EXPECT_EQ(fa.redirect_server.redirect_server_address(), "google.com");
  EXPECT_EQ(
      fa.redirect_server.redirect_address_type(),
      RedirectServer_RedirectAddressType_URL);
}

// We want to test a case where we do not receive a GSU, but we receive a
// final_action on credit exhaust.
TEST_F(SessionStateTest, test_empty_credit_grant) {
  insert_rule(1, "m1", "rule1", STATIC, 0, 0);
  CreditUpdateResponse charge_resp;
  charge_resp.set_success(true);
  charge_resp.set_sid("IMSI1");
  charge_resp.set_charging_key(1);

  // A ChargingCredit with no GSU but FinalAction
  auto p_credit = charge_resp.mutable_credit();
  p_credit->set_type(ChargingCredit::BYTES);
  p_credit->set_is_final(true);
  p_credit->set_final_action(ChargingCredit_FinalAction_TERMINATE);

  session_state->receive_charging_credit(charge_resp, update_criteria);

  // Test that the update criteria is filled out properly
  EXPECT_EQ(update_criteria.charging_credit_to_install.size(), 1);
  auto u_credit = update_criteria.charging_credit_to_install[1];
  EXPECT_TRUE(u_credit.is_final);
  EXPECT_EQ(
      u_credit.final_action_info.final_action,
      ChargingCredit_FinalAction_TERMINATE);

  // At this point, the charging credit for RG=1 should have no available quota
  // and the tracking should be the default, TOTAL_ONLY
  EXPECT_EQ(u_credit.credit.grant_tracking_type, TOTAL_ONLY);
  EXPECT_EQ(u_credit.credit.buckets[ALLOWED_TOTAL], 0);
  EXPECT_EQ(u_credit.credit.buckets[ALLOWED_TX], 0);
  EXPECT_EQ(u_credit.credit.buckets[ALLOWED_RX], 0);

  // Report some rule usage, and ensure the final action gets triggered
  session_state->add_rule_usage("rule1", 100, 100, update_criteria);
  auto credit_uc = update_criteria.charging_credit_map[1];
  EXPECT_EQ(credit_uc.service_state, SERVICE_NEEDS_DEACTIVATION);
}

TEST_F(SessionStateTest, test_multiple_final_action_empty_grant) {
  // add one rule with credites
  insert_rule(1, "", "rule1", STATIC, 0, 0);
  EXPECT_TRUE(
      std::find(
          update_criteria.static_rules_to_install.begin(),
          update_criteria.static_rules_to_install.end(),
          "rule1") != update_criteria.static_rules_to_install.end());

  receive_credit_from_ocs(1, 3000, 2000, 2000, false);
  EXPECT_EQ(update_criteria.charging_credit_to_install.size(), 1);
  EXPECT_EQ(
      update_criteria.charging_credit_to_install[CreditKey(1)]
          .credit.buckets[ALLOWED_TOTAL],3000);
  EXPECT_EQ(
      update_criteria.charging_credit_to_install[CreditKey(1)]
          .credit.buckets[ALLOWED_TX],2000);
  EXPECT_EQ(
      update_criteria.charging_credit_to_install[CreditKey(1)]
          .credit.buckets[ALLOWED_RX],2000);

  // add usage for 2 times to go over quota
  session_state->add_rule_usage("rule1", 2000, 1000, update_criteria);
  EXPECT_EQ(session_state->get_charging_credit(1, USED_TX), 2000);
  EXPECT_EQ(session_state->get_charging_credit(1, USED_RX), 1000);

  session_state->add_rule_usage("rule1", 2000, 1000, update_criteria);
  EXPECT_EQ(session_state->get_charging_credit(1, USED_TX), 4000);
  EXPECT_EQ(session_state->get_charging_credit(1, USED_RX), 2000);

  // check if we need to report the usage
  UpdateSessionRequest update;
  std::vector<std::unique_ptr<ServiceAction>> actions;
  session_state->get_updates(update, &actions, update_criteria);
  EXPECT_EQ(actions.size(), 0);
  EXPECT_EQ(update.updates_size(), 1);
  EXPECT_EQ(update_criteria.charging_credit_map[CreditKey(1)].bucket_deltas[USED_TX],4000);
  EXPECT_EQ(update_criteria.charging_credit_map[CreditKey(1)].bucket_deltas[USED_RX], 2000);
  EXPECT_EQ(update_criteria.charging_credit_map[CreditKey(1)].service_state, SERVICE_ENABLED);
  EXPECT_FALSE(update_criteria.charging_credit_map[CreditKey(1)].is_final);
  EXPECT_TRUE(update_criteria.charging_credit_map[CreditKey(1)].reporting);

  // recive final unit without grant
  receive_credit_from_ocs(1, 0, 0, 0, true);
  EXPECT_EQ(update_criteria.charging_credit_to_install.size(), 1);
  EXPECT_EQ(update_criteria.charging_credit_map[CreditKey(1)].bucket_deltas[REPORTED_TX], 4000);
  EXPECT_EQ(update_criteria.charging_credit_map[CreditKey(1)].bucket_deltas[REPORTED_RX], 2000);
  EXPECT_TRUE(update_criteria.charging_credit_map[CreditKey(1)].is_final);
  EXPECT_EQ(update_criteria.charging_credit_map[CreditKey(1)].service_state, SERVICE_ENABLED);
  EXPECT_FALSE(update_criteria.charging_credit_map[CreditKey(1)].reporting);

  // force to check for the state (no traffic sent)
  session_state->add_rule_usage("rule1", 0, 0, update_criteria);
  EXPECT_EQ(session_state->get_charging_credit(1, USED_TX), 4000);
  EXPECT_EQ(session_state->get_charging_credit(1, USED_RX), 2000);
  EXPECT_TRUE(update_criteria.charging_credit_map[CreditKey(1)].is_final);
  EXPECT_EQ(update_criteria.charging_credit_map[CreditKey(1)].service_state, SERVICE_NEEDS_DEACTIVATION);
  EXPECT_FALSE(update_criteria.charging_credit_map[CreditKey(1)].reporting);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  FLAGS_logtostderr = 1;
  FLAGS_v           = 10;
  return RUN_ALL_TESTS();
}

}  // namespace magma
