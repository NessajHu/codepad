#pragma once

#include <cassert>
#include <list>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

namespace codepad {
	class callback_buffer {
	public:
		template <typename T> void add(T &&func) {
			std::lock_guard<std::mutex> guard(_lock);
			_cbs.emplace_back(std::forward<T>(func));
		}
		void flush() {
			std::lock_guard<std::mutex> guard(_lock);
			if (_cbs.size() > 0) {
				for (auto i = _cbs.begin(); i != _cbs.end(); ++i) {
					(*i)();
				}
				_cbs.clear();
			}
		}

		static callback_buffer &get();
	protected:
		std::mutex _lock;
		std::vector<std::function<void()>> _cbs;
	};

	/* pool thread:
	 *   not_initiated -> cancel_requested
	 *   running       -> cancel_requested
	 *
	 * task thread:
	 *   not_initiated    -> running
	 *   running          -> completed
	 *   cancel_requested -> cancelled
	 */
	enum class task_status {
		not_initiated, // TODO useful?
		running,
		cancel_requested,
		completed,
		cancelled
	};
	class async_task_pool {
	public:
#ifndef NDEBUG
		async_task_pool() : _creator(std::this_thread::get_id()) {
		}
#endif

		struct async_task {
			friend class async_task_pool;
		public:
			typedef std::function<void(async_task&)> operation_t;

			explicit async_task(operation_t f) : operation(std::move(f)) {
			}

			task_status get_status() const {
				return _st;
			}
			bool is_cancel_requested() const {
				return _st == task_status::cancel_requested;
			}
			bool is_finished() const {
				return _st == task_status::completed || _st == task_status::cancelled;
			}

			template <typename T, typename Func> bool acquire_data(T &obj, const Func &f) {
				std::atomic_bool bv(false);
				callback_buffer::get().add([&bv, &obj, &f]() {
					obj = f();
					bv = true;
				});
				while (!bv) {
					if (is_cancel_requested()) {
						return false;
					}
				}
				return true;
			}

			const operation_t operation;
		protected:
			std::atomic<task_status> _st{task_status::not_initiated};

			void _run() {
				task_status exp = task_status::not_initiated;
				if (_st.compare_exchange_strong(exp, task_status::running)) {
					operation(*this);
					exp = task_status::running;
					if (!_st.compare_exchange_strong(exp, task_status::completed)) {
						_st = task_status::cancelled;
					}
				} else {
					assert(exp == task_status::cancel_requested);
					_st = task_status::cancelled;
				}
			}
		};
		typedef typename std::list<async_task>::iterator token;

		template <typename T, typename ...Args> token run_task(T &&func, Args &&...args) {
			assert(std::this_thread::get_id() == _creator);
			_lst.emplace_back(async_task::operation_t(std::bind(
				std::forward<T>(func), std::forward<Args>(args)..., std::placeholders::_1
			)));
			token tk = --_lst.end();
			std::thread t(std::bind(&async_task::_run, &*tk));
			t.detach();
			return tk;
		}
		bool try_cancel(token t) {
			task_status exp = task_status::not_initiated;
			if (!t->_st.compare_exchange_strong(exp, task_status::cancel_requested)) {
				exp = task_status::running;
				if (!t->_st.compare_exchange_strong(exp, task_status::cancel_requested)) {
					return false;
				}
			}
			return true;
		}
		bool try_finish(token t) {
			task_status dummy;
			return try_finish(t, dummy);
		}
		bool try_finish(token t, task_status &st) {
			st = t->_st;
			if (st == task_status::cancelled || st == task_status::completed) {
				_lst.erase(t);
				return true;
			}
			return false;
		}
		task_status wait_finish(token t) {
			while (!t->is_finished()) {
			}
			task_status fstat = t->get_status();
			_lst.erase(t);
			return fstat;
		}

		std::list<async_task> &tasks() {
			return _lst;
		}

		static async_task_pool &get();
	protected:
		std::list<async_task> _lst;
#ifndef NDEBUG
		std::thread::id _creator;
#endif
	};
}
