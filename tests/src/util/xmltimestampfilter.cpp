/***************************************************************************
                            xmltimestampfilter.cpp
                             -------------------
    begin                : 2003/12/11
    copyright            : (C) 2003 by Michael CATANZARITI
    email                : mcatan@free.fr
 ***************************************************************************/
 /***************************************************************************
 * Copyright (C) The Apache Software Foundation. All rights reserved.      *
 *                                                                         *
 * This software is published under the terms of the Apache Software       *
 * License version 1.1, a copy of which has been included with this        *
 * distribution in the license.apl file.                                   *
 ***************************************************************************/

#include "xmltimestampfilter.h"

using namespace log4cxx;
using namespace log4cxx::helpers;

String XMLTimestampFilter::filter(const String& in)
	const throw(UnexpectedFormatException)
{
	return merge(_T("timestamp=\"\\d{10,13}\""), in, _T("timestamp=\"XXX\""));
}

