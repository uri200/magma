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

#include <string>
#include "magma_logging.h"
#include "GrpcMagmaUtils.h"

#define MAGMA_PRINT_GRPC_PAYLOAD "MAGMA_PRINT_GRPC_PAYLOAD"

std::string grpcLoginLevel = get_env_var(MAGMA_PRINT_GRPC_PAYLOAD);

std::string get_env_var(std::string const& key) {
  MLOG(MINFO) << "Checking env var " << key;
  char* val;
  val                = getenv(key.c_str());
  std::string retval = "";
  if (val != NULL) {
    retval = val;
  }
  return std::string(retval);
}

void PrintGrpcMessage(const google::protobuf::Message& msg) {
  if (grpcLoginLevel == "1") {
    // Lazy log strategy
    const google::protobuf::Descriptor* desc = msg.GetDescriptor();
    MLOG(MINFO) << "\n"
                << "  " << desc->full_name().c_str() << " {\n"
                << indentText(msg.DebugString(), 6) << "  }";
  }
}

std::string indentText(std::string basicString, int indent) {
  std::stringstream iss(basicString);
  std::string blanks(indent, ' ');
  std::string result = "";
  while (iss.good()) {
    std::string SingleLine;
    getline(iss, SingleLine, '\n');
    // skip empty lines
    if (SingleLine == "") {
      continue;
    }
    result += blanks;
    result += SingleLine;
    // do not add \n on the last line
    result += "\n";
  }
  return result;
}
