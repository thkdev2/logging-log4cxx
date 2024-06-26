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


#include <log4cxx/asyncappender.h>


#include <log4cxx/helpers/loglog.h>
#include <log4cxx/spi/loggingevent.h>
#include <log4cxx/helpers/stringhelper.h>
#include <log4cxx/helpers/optionconverter.h>
#include <log4cxx/helpers/threadutility.h>
#include <log4cxx/private/appenderskeleton_priv.h>
#include <thread>
#include <atomic>
#include <condition_variable>

#if LOG4CXX_EVENTS_AT_EXIT
#include <log4cxx/private/atexitregistry.h>
#endif

using namespace LOG4CXX_NS;
using namespace LOG4CXX_NS::helpers;
using namespace LOG4CXX_NS::spi;

#if 15 < LOG4CXX_ABI_VERSION
namespace
{
#endif

/**
 * The default buffer size is set to 128 events.
*/
enum { DEFAULT_BUFFER_SIZE = 128 };

class DiscardSummary
{
	private:
		/**
		 * First event of the highest severity.
		*/
		::LOG4CXX_NS::spi::LoggingEventPtr maxEvent;

		/**
		* Total count of messages discarded.
		*/
		int count;

	public:
		/**
		 * Create new instance.
		 *
		 * @param event event, may not be null.
		*/
		DiscardSummary(const ::LOG4CXX_NS::spi::LoggingEventPtr& event);
		/** Copy constructor.  */
		DiscardSummary(const DiscardSummary& src);
		/** Assignment operator. */
		DiscardSummary& operator=(const DiscardSummary& src);

		/**
		 * Add discarded event to summary.
		 *
		 * @param event event, may not be null.
		*/
		void add(const ::LOG4CXX_NS::spi::LoggingEventPtr& event);

		/**
		 * Create event with summary information.
		 *
		 * @return new event.
		 */
		::LOG4CXX_NS::spi::LoggingEventPtr createEvent(::LOG4CXX_NS::helpers::Pool& p);

		static
		::LOG4CXX_NS::spi::LoggingEventPtr createEvent(::LOG4CXX_NS::helpers::Pool& p,
			size_t discardedCount);
};

typedef std::map<LogString, DiscardSummary> DiscardMap;

#if 15 < LOG4CXX_ABI_VERSION
}
#endif

static const int CACHE_LINE_SIZE = 128;

struct AsyncAppender::AsyncAppenderPriv : public AppenderSkeleton::AppenderSkeletonPrivate
{
	AsyncAppenderPriv() :
		AppenderSkeletonPrivate(),
		buffer(DEFAULT_BUFFER_SIZE),
		bufferSize(DEFAULT_BUFFER_SIZE),
		appenders(pool),
		dispatcher(),
		locationInfo(false),
		blocking(true)
#if LOG4CXX_EVENTS_AT_EXIT
		, atExitRegistryRaii([this]{atExitActivated();})
#endif
		, eventCount(0)
		, dispatchedCount(0)
		, commitCount(0)
	{
	}

#if LOG4CXX_EVENTS_AT_EXIT
	void atExitActivated()
	{
		std::unique_lock<std::mutex> lock(bufferMutex);
		bufferNotFull.wait(lock, [this]() -> bool
			{ return buffer.empty() || closed; }
		);
	}
#endif

	/**
	 * Event buffer.
	*/
	LoggingEventList buffer;

	/**
	 *  Mutex used to guard access to buffer and discardMap.
	 */
	std::mutex bufferMutex;

	std::condition_variable bufferNotFull;
	std::condition_variable bufferNotEmpty;

	/**
	  * Map of DiscardSummary objects keyed by logger name.
	*/
	DiscardMap discardMap;

	/**
	 * The maximum number of undispatched events.
	*/
	int bufferSize;

	/**
	 * Nested appenders.
	*/
	helpers::AppenderAttachableImpl appenders;

	/**
	 *  Dispatcher.
	 */
	std::thread dispatcher;

	/**
	 * Should location info be included in dispatched messages.
	*/
	bool locationInfo;

	/**
	 * Does appender block when buffer is full.
	*/
	bool blocking;

#if LOG4CXX_EVENTS_AT_EXIT
	helpers::AtExitRegistry::Raii atExitRegistryRaii;
#endif

	/**
	 * Used to calculate the buffer position at which to store the next event.
	*/
	alignas(CACHE_LINE_SIZE) std::atomic<size_t> eventCount;

	/**
	 * Used to calculate the buffer position from which to extract the next event.
	*/
	alignas(CACHE_LINE_SIZE) std::atomic<size_t> dispatchedCount;

	/**
	 * Used to communicate to the dispatch thread when an event is committed in buffer.
	*/
	alignas(CACHE_LINE_SIZE) std::atomic<size_t> commitCount;
};


IMPLEMENT_LOG4CXX_OBJECT(AsyncAppender)

#define priv static_cast<AsyncAppenderPriv*>(m_priv.get())

AsyncAppender::AsyncAppender()
	: AppenderSkeleton(std::make_unique<AsyncAppenderPriv>())
{
}

AsyncAppender::~AsyncAppender()
{
	finalize();
}

void AsyncAppender::addAppender(const AppenderPtr newAppender)
{
	priv->appenders.addAppender(newAppender);
}


void AsyncAppender::setOption(const LogString& option,
	const LogString& value)
{
	if (StringHelper::equalsIgnoreCase(option, LOG4CXX_STR("LOCATIONINFO"), LOG4CXX_STR("locationinfo")))
	{
		setLocationInfo(OptionConverter::toBoolean(value, false));
	}

	if (StringHelper::equalsIgnoreCase(option, LOG4CXX_STR("BUFFERSIZE"), LOG4CXX_STR("buffersize")))
	{
		setBufferSize(OptionConverter::toInt(value, DEFAULT_BUFFER_SIZE));
	}

	if (StringHelper::equalsIgnoreCase(option, LOG4CXX_STR("BLOCKING"), LOG4CXX_STR("blocking")))
	{
		setBlocking(OptionConverter::toBoolean(value, true));
	}
	else
	{
		AppenderSkeleton::setOption(option, value);
	}
}


void AsyncAppender::doAppend(const spi::LoggingEventPtr& event, Pool& pool1)
{
	doAppendImpl(event, pool1);
}

void AsyncAppender::append(const spi::LoggingEventPtr& event, Pool& p)
{
	if (priv->bufferSize <= 0)
	{
		priv->appenders.appendLoopOnAppenders(event, p);
	}

	// Set the NDC and MDC for the calling thread as these
	// LoggingEvent fields were not set at event creation time.
	LogString ndcVal;
	event->getNDC(ndcVal);
	// Get a copy of this thread's MDC.
	event->getMDCCopy();

	if (!priv->dispatcher.joinable())
	{
		std::unique_lock<std::mutex> lock(priv->bufferMutex);
		if (!priv->dispatcher.joinable())
			priv->dispatcher = ThreadUtility::instance()->createThread( LOG4CXX_STR("AsyncAppender"), &AsyncAppender::dispatch, this );
	}
	while (true)
	{
		auto pendingCount = priv->eventCount - priv->dispatchedCount;
		if (0 <= pendingCount && pendingCount < priv->bufferSize)
		{
			// Claim a slot in the ring buffer
			auto oldEventCount = priv->eventCount++;
			auto index = oldEventCount % priv->buffer.size();
			// Wait for a free slot
			while (priv->bufferSize <= oldEventCount - priv->dispatchedCount)
				;
			// Write to the ring buffer
			priv->buffer[index] = event;
			// Notify the dispatch thread that an event has been added
			auto savedEventCount = oldEventCount;
			while (!priv->commitCount.compare_exchange_weak(oldEventCount, oldEventCount + 1, std::memory_order_release))
			{
				 oldEventCount = savedEventCount;
			}
			priv->bufferNotEmpty.notify_all();
			break;
		}
		//
		//   Following code is only reachable if buffer is full or eventCount has overflowed
		//
		std::unique_lock<std::mutex> lock(priv->bufferMutex);
		//
		//   if blocking and thread is not already interrupted
		//      and not the dispatcher then
		//      wait for a buffer notification
		bool discard = true;

		if (priv->blocking
			&& !priv->closed
			&& (priv->dispatcher.get_id() != std::this_thread::get_id()) )
		{
			priv->bufferNotFull.wait(lock, [this]()
			{
				return priv->eventCount - priv->dispatchedCount < priv->bufferSize;
			});
			discard = false;
		}

		//
		//   if blocking is false or thread has been interrupted
		//   add event to discard map.
		//
		if (discard)
		{
			LogString loggerName = event->getLoggerName();
			DiscardMap::iterator iter = priv->discardMap.find(loggerName);

			if (iter == priv->discardMap.end())
			{
				DiscardSummary summary(event);
				priv->discardMap.insert(DiscardMap::value_type(loggerName, summary));
			}
			else
			{
				(*iter).second.add(event);
			}

			break;
		}
	}
}

void AsyncAppender::close()
{
	{
		std::lock_guard<std::mutex> lock(priv->bufferMutex);
		priv->closed = true;
		priv->bufferNotEmpty.notify_all();
		priv->bufferNotFull.notify_all();
	}

	if ( priv->dispatcher.joinable() )
	{
		priv->dispatcher.join();
	}

	for (auto item : priv->appenders.getAllAppenders())
	{
		item->close();
	}
}

AppenderList AsyncAppender::getAllAppenders() const
{
	return priv->appenders.getAllAppenders();
}

AppenderPtr AsyncAppender::getAppender(const LogString& n) const
{
	return priv->appenders.getAppender(n);
}

bool AsyncAppender::isAttached(const AppenderPtr appender) const
{
	return priv->appenders.isAttached(appender);
}

bool AsyncAppender::requiresLayout() const
{
	return false;
}

void AsyncAppender::removeAllAppenders()
{
	priv->appenders.removeAllAppenders();
}

void AsyncAppender::removeAppender(const AppenderPtr appender)
{
	priv->appenders.removeAppender(appender);
}

void AsyncAppender::removeAppender(const LogString& n)
{
	priv->appenders.removeAppender(n);
}

bool AsyncAppender::getLocationInfo() const
{
	return priv->locationInfo;
}

void AsyncAppender::setLocationInfo(bool flag)
{
	priv->locationInfo = flag;
}


void AsyncAppender::setBufferSize(int size)
{
	if (size < 0)
	{
		throw IllegalArgumentException(LOG4CXX_STR("size argument must be non-negative"));
	}

	std::lock_guard<std::mutex> lock(priv->bufferMutex);
	priv->bufferSize = (size < 1) ? 1 : size;
	priv->buffer.resize(priv->bufferSize);
	priv->bufferNotFull.notify_all();
}

int AsyncAppender::getBufferSize() const
{
	return priv->bufferSize;
}

void AsyncAppender::setBlocking(bool value)
{
	std::lock_guard<std::mutex> lock(priv->bufferMutex);
	priv->blocking = value;
	priv->bufferNotFull.notify_all();
}

bool AsyncAppender::getBlocking() const
{
	return priv->blocking;
}

DiscardSummary::DiscardSummary(const LoggingEventPtr& event) :
	maxEvent(event), count(1)
{
}

DiscardSummary::DiscardSummary(const DiscardSummary& src) :
	maxEvent(src.maxEvent), count(src.count)
{
}

DiscardSummary& DiscardSummary::operator=(const DiscardSummary& src)
{
	maxEvent = src.maxEvent;
	count = src.count;
	return *this;
}

void DiscardSummary::add(const LoggingEventPtr& event)
{
	if (event->getLevel()->toInt() > maxEvent->getLevel()->toInt())
	{
		maxEvent = event;
	}

	count++;
}

LoggingEventPtr DiscardSummary::createEvent(Pool& p)
{
	LogString msg(LOG4CXX_STR("Discarded "));
	StringHelper::toString(count, p, msg);
	msg.append(LOG4CXX_STR(" messages due to a full event buffer including: "));
	msg.append(maxEvent->getMessage());
	return std::make_shared<LoggingEvent>(
				maxEvent->getLoggerName(),
				maxEvent->getLevel(),
				msg,
				LocationInfo::getLocationUnavailable() );
}

::LOG4CXX_NS::spi::LoggingEventPtr
DiscardSummary::createEvent(::LOG4CXX_NS::helpers::Pool& p,
	size_t discardedCount)
{
	LogString msg(LOG4CXX_STR("Discarded "));
	StringHelper::toString(discardedCount, p, msg);
	msg.append(LOG4CXX_STR(" messages due to a full event buffer"));

	return std::make_shared<LoggingEvent>(
				LOG4CXX_STR(""),
				LOG4CXX_NS::Level::getError(),
				msg,
				LocationInfo::getLocationUnavailable() );
}

void AsyncAppender::dispatch()
{
	bool isActive = true;

	while (isActive)
	{
		Pool p;
		LoggingEventList events;
		events.reserve(priv->bufferSize);
		//
		//   process events after lock on buffer is released.
		//
		{
			std::unique_lock<std::mutex> lock(priv->bufferMutex);
			priv->bufferNotEmpty.wait(lock, [this]() -> bool
				{ return priv->dispatchedCount != priv->commitCount || priv->closed; }
			);
			isActive = !priv->closed;

			while (events.size() < priv->bufferSize && priv->dispatchedCount != priv->commitCount)
			{
				auto index = priv->dispatchedCount % priv->buffer.size();
				events.push_back(priv->buffer[index]);
				++priv->dispatchedCount;
			}
			for (auto discardItem : priv->discardMap)
			{
				events.push_back(discardItem.second.createEvent(p));
			}

			priv->discardMap.clear();
			priv->bufferNotFull.notify_all();
		}

		for (auto item : events)
		{
			try
			{
				priv->appenders.appendLoopOnAppenders(item, p);
			}
			catch (std::exception& ex)
			{
				if (isActive)
				{
					priv->errorHandler->error(LOG4CXX_STR("async dispatcher"), ex, 0, item);
					isActive = false;
				}
			}
			catch (...)
			{
				if (isActive)
				{
					priv->errorHandler->error(LOG4CXX_STR("async dispatcher"));
					isActive = false;
				}
			}
		}
	}

}
