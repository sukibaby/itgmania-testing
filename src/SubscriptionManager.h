/* SubscriptionManager - Object that accepts subscriptions. */

#ifndef SubscriptionManager_H
#define SubscriptionManager_H

#include <set>
#include <vector>

// Since this class is trivially constructible, there's no initialization
// order problem. The std::set<T*> itself is not trivially constructible,
// so we use a function-local static to avoid static initialization order
// issues, and we rely on the std::set destructor to clean up at program exit.
template <class T>
class SubscriptionManager {
 public:
  using container_t = std::set<T*>;

  // TRICKY: Avoid static initialization order issues by storing the
  // container as a function-local static (constructed on first use).
  static container_t& Subscribers() {
    static container_t s;
    return s;
  }

  container_t& Get() { return Subscribers(); }
  const container_t& Get() const { return Subscribers(); }

  bool Empty() const { return Subscribers().empty(); }

  // Return a copy of the subscriber list, rather than a direct
  // reference to the internal container. This allows us to iterate
  // without worrying about locking or if the container is being modified.
  std::vector<T*> Snapshot() const {
    const auto& s = Subscribers();
    return std::vector<T*>(s.begin(), s.end());
  }

  void Subscribe(T* p) {
    auto& s = Subscribers();
#ifdef DEBUG
		typename container_t::iterator iter = s.find( p );
		ASSERT_M( iter == s.end(), "already subscribed" );
#endif
		s.insert( p );
	}

	void Unsubscribe( T* p )
	{
		auto &s = Subscribers();
		typename container_t::iterator iter = s.find( p );
		ASSERT( iter != s.end() );
		s.erase( iter );
	}

	bool IsSubscriberListPopulated() const { return !Subscribers().empty(); }
};

#endif

/*
 * (c) 2004-2005 Chris Danford
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
