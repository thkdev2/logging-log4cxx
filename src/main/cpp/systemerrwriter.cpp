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

#include <log4cxx/logstring.h>
#include <log4cxx/helpers/systemerrwriter.h>
#include <log4cxx/helpers/transcoder.h>
#include <stdio.h>
#if !defined(LOG4CXX)
	#define LOG4CXX 1
#endif
#include <log4cxx/private/log4cxx_private.h>

using namespace LOG4CXX_NS;
using namespace LOG4CXX_NS::helpers;

IMPLEMENT_LOG4CXX_OBJECT(SystemErrWriter)

SystemErrWriter::SystemErrWriter()
{
}

SystemErrWriter::~SystemErrWriter()
{
}

void SystemErrWriter::close(Pool& /* p */)
{
}

void SystemErrWriter::flush(Pool& /* p */)
{
	flush();
}

void SystemErrWriter::write(const LogString& str, Pool& /* p */)
{
	write(str);
}

bool SystemErrWriter::isWide()
{
#if LOG4CXX_FORCE_WIDE_CONSOLE
	return true;
#elif LOG4CXX_FORCE_BYTE_CONSOLE || !LOG4CXX_HAS_FWIDE
	return false;
#else
	return fwide(stderr, 0) > 0;
#endif
}

void SystemErrWriter::write(const LogString& str)
{
#if LOG4CXX_WCHAR_T_API

	if (isWide())
	{
		LOG4CXX_ENCODE_WCHAR(msg, str);
		fputws(msg.c_str(), stderr);
		return;
	}

#endif
	LOG4CXX_ENCODE_CHAR(msg, str);
	fputs(msg.c_str(), stderr);
}

void SystemErrWriter::flush()
{
	fflush(stderr);
}
