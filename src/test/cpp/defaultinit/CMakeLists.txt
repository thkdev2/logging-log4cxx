#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

if(NOT LOG4CXX_DOMCONFIGURATOR_SUPPORT)
    add_executable(defaultinittestcase testcase1.cpp testcase3.cpp)
else()
    add_executable(defaultinittestcase testcase1.cpp testcase2.cpp testcase3.cpp testcase4.cpp)
endif()

set(ALL_LOG4CXX_TESTS ${ALL_LOG4CXX_TESTS} defaultinittestcase PARENT_SCOPE)
