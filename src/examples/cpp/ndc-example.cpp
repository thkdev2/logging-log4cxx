/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdlib.h>
#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/ndc.h>
#include <locale.h>

using namespace log4cxx;

int main()
{
    setlocale(LC_ALL, ""); // Use the user-preferred locale
    int result = EXIT_SUCCESS;
    try
    {
        BasicConfigurator::configure();
        LoggerPtr rootLogger = Logger::getRootLogger();

        NDC context1("[outer]");
        LOG4CXX_INFO(rootLogger, "info message"); // log entry contains "[outer]"
        {
            NDC context2("[inner]");
            LOG4CXX_DEBUG(rootLogger, "debug message"); // log entry contains "[outer] [inner]"
        }
        LOG4CXX_WARN(rootLogger, "warn message"); // log entry contains "[outer]"
    }
    catch(std::exception&)
    {
        result = EXIT_FAILURE;
    }

    return result;
}
