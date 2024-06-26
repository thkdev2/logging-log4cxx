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

#include "vectorappender.h"
#include <thread>

using namespace log4cxx;
using namespace log4cxx::helpers;

IMPLEMENT_LOG4CXX_OBJECT(VectorAppender)

void VectorAppender::append(const spi::LoggingEventPtr& event, Pool& /*p*/)
{
	if (0 < this->appendMillisecondDelay)
		std::this_thread::sleep_for( std::chrono::milliseconds( this->appendMillisecondDelay ) );
	this->vector.push_back(event);
}

void VectorAppender::close()
{
	if (m_priv->closed)
	{
		return;
	}

	m_priv->closed = true;
}
