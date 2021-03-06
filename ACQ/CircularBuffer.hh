#ifndef _ACQ_CircularBuffer_hh_
#define _ACQ_CircularBuffer_hh_
#include <boost/circular_buffer.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/thread.hpp>
#include <boost/call_traits.hpp>
#include <boost/bind.hpp>

namespace acq {

// Class from boost examples
template <class T>
class bounded_buffer
{
public:

  typedef boost::circular_buffer<T> container_type;
  typedef typename container_type::size_type size_type;
  typedef typename container_type::value_type value_type;
  typedef typename boost::call_traits<value_type>::param_type param_type;

  explicit bounded_buffer(size_type capacity) : m_unread(0), m_container(capacity) {}

  void push_front(typename boost::call_traits<value_type>::param_type item)
  { // `param_type` represents the "best" way to pass a parameter of type `value_type` to a method.

      boost::mutex::scoped_lock lock(m_mutex);
      m_not_full.wait(lock, boost::bind(&bounded_buffer<value_type>::is_not_full, this));
      m_container.push_front(item);
      ++m_unread;
      lock.unlock();
      m_not_empty.notify_one();
  }

  void push(typename boost::call_traits<value_type>::param_type item)
  {
      push_front(item);
  }
  bool pop_back(value_type* pItem) {
      boost::mutex::scoped_lock lock(m_mutex);
      if (!m_not_empty.timed_wait(lock,
          boost::posix_time::milliseconds(10),
          [this] () { return is_not_empty(); })) {
          return false;
      }
      *pItem = m_container[--m_unread];
      lock.unlock();
      m_not_full.notify_one();
      return true;
  }

  template <typename Functor>
  bool consume_one(Functor & f)
  {
      value_type element;
      if (!pop_back(&element)) {
          return false;
      }
      f(element);
      return true;
  }

  template <typename Functor>
  size_t consume_all_no_lock(Functor & f)
  {
      size_t element_count = 0;
      while (is_not_empty()) {
        f(m_container[--m_unread]);
        element_count += 1;
      }
      return element_count;
  }


  template <typename Functor>
  size_t consume_all(Functor & f)
  {
      boost::mutex::scoped_lock lock(m_mutex);
      return consume_all_no_lock(f);
  }

private:
  bounded_buffer(const bounded_buffer&);              // Disabled copy constructor.
  bounded_buffer& operator = (const bounded_buffer&); // Disabled assign operator.

  bool is_not_empty() const { return m_unread > 0; }
  bool is_not_full() const { return m_unread < m_container.capacity(); }

  size_type m_unread;
  container_type m_container;
  boost::mutex m_mutex;
  boost::condition m_not_empty;
  boost::condition m_not_full;
}; //

}

#endif /* _ACQ_CircularBuffer_hh_ */
