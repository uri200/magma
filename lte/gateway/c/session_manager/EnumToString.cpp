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

#include "EnumToString.h"

namespace magma {
std::string reauth_state_to_str(ReAuthState state) {
  switch (state) {
    case REAUTH_NOT_NEEDED:
      return "REAUTH_NOT_NEEDED";
    case REAUTH_REQUIRED:
      return "REAUTH_REQUIRED";
    case REAUTH_PROCESSING:
      return "REAUTH_PROCESSING";
    default:
      return "INVALID REAUTH STATE";
  }
}

std::string service_state_to_str(ServiceState state) {
  switch (state) {
    case SERVICE_ENABLED:
      return "SERVICE_ENABLED";
    case SERVICE_NEEDS_DEACTIVATION:
      return "SERVICE_NEEDS_DEACTIVATION";
    case SERVICE_DISABLED:
      return "SERVICE_DISABLED";
    case SERVICE_NEEDS_ACTIVATION:
      return "SERVICE_NEEDS_ACTIVATION";
    case SERVICE_REDIRECTED:
      return "SERVICE_REDIRECTED";
    case SERVICE_RESTRICTED:
      return "SERVICE_RESTRICTED";
    default:
      return "INVALID SERVICE STATE";
  }
}

std::string final_action_to_str(ChargingCredit_FinalAction final_action) {
  switch (final_action) {
    case ChargingCredit_FinalAction_TERMINATE:
      return "TERMINATE";
    case ChargingCredit_FinalAction_REDIRECT:
      return "REDIRECT";
    case ChargingCredit_FinalAction_RESTRICT_ACCESS:
      return "RESTRICT_ACCESS";
    default:
      return "INVALID FINAL ACTION";
  }
}

std::string grant_type_to_str(GrantTrackingType grant_type) {
  switch (grant_type) {
    case ALL_TOTAL_TX_RX:
      return "ALL_TOTAL_TX_RX (Total, Rx, Tx)";
    case TOTAL_ONLY:
      return "TOTAL_ONLY";
    case TX_ONLY:
      return "TX_ONLY";
    case RX_ONLY:
      return "RX_ONLY";
    case TX_AND_RX:
      return "TX_AND_RX";
    default:
      return "INVALID GRANT TRACKING TYPE";
  }
}

std::string session_fsm_state_to_str(SessionFsmState state) {
  switch (state) {
    case SESSION_ACTIVE:
      return "SESSION_ACTIVE";
    case SESSION_TERMINATING_FLOW_ACTIVE:
      return "SESSION_TERMINATING_FLOW_ACTIVE";
    case SESSION_TERMINATING_AGGREGATING_STATS:
      return "SESSION_TERMINATING_AGGREGATING_STATS";
    case SESSION_TERMINATING_FLOW_DELETED:
      return "SESSION_TERMINATING_FLOW_DELETED";
    case SESSION_TERMINATED:
      return "SESSION_TERMINATED";
    case SESSION_TERMINATION_SCHEDULED:
      return "SESSION_TERMINATION_SCHEDULED";
    default:
      return "INVALID SESSION FSM STATE";
  }
}

std::string credit_update_type_to_str(CreditUsage::UpdateType update) {
  switch (update) {
    case CreditUsage::THRESHOLD:
      return "THRESHOLD";
    case CreditUsage::QHT:
      return "QHT";
    case CreditUsage::TERMINATED:
      return "TERMINATED";
    case CreditUsage::QUOTA_EXHAUSTED:
      return "QUOTA_EXHAUSTED";
    case CreditUsage::VALIDITY_TIMER_EXPIRED:
      return "VALIDITY_TIMER_EXPIRED";
    case CreditUsage::OTHER_QUOTA_TYPE:
      return "OTHER_QUOTA_TYPE";
    case CreditUsage::RATING_CONDITION_CHANGE:
      return "RATING_CONDITION_CHANGE";
    case CreditUsage::REAUTH_REQUIRED:
      return "REAUTH_REQUIRED";
    case CreditUsage::POOL_EXHAUSTED:
      return "POOL_EXHAUSTED";
    default:
      return "INVALID CREDIT UPDATE TYPE";
  }
}

std::string raa_result_to_str(ReAuthResult res) {
  switch (res) {
    case UPDATE_INITIATED:
      return "UPDATE_INITIATED";
    case UPDATE_NOT_NEEDED:
      return "UPDATE_NOT_NEEDED";
    case SESSION_NOT_FOUND:
      return "SESSION_NOT_FOUND";
    case OTHER_FAILURE:
      return "OTHER_FAILURE";
    default:
      return "UNKNOWN_RESULT";
  }
}
}  // namespace magma
