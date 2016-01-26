#include"../header/CThreadPool.hpp"
#include<memory>	//make_unique
#include<mutex>
#include<vector>
#include<unordered_map>
#include"../../lib/header/thread/CThreadList.hpp"
#include"../../lib/header/thread/CThreadQueue.hpp"
#include"../header/CThreadPoolItem.hpp"
#include"../header/IThreadPoolCommun.hpp"
#include"../header/IThreadPoolItemExecutor.hpp"
using namespace std;

namespace nThread
{
	struct CThreadPool::Impl
	{
		unordered_map<thread::id,bool> is_joinable;
		CThreadList<CThreadPoolItem*> join_anyList;
		mutex join_all_mut;	//only for join_all
		mutex wait_until_all_available_mut;	//only for wait_until_all_available
		CThreadQueue<CThreadPoolItem*> waitingQue;
		unordered_map<thread::id,CThreadPoolItem> thr;
		Impl(size_t);
		CThreadPool::thread_id add(function<void()> &&);
		void add_and_detach(function<void()> &&);
		void join_all();
		bool joinable(thread::id) const noexcept;
		void wait_until_all_available();
	};

	CThreadPool::Impl::Impl(size_t size)
	{
		while(size--)
		{
			CThreadPoolItem item;
			const auto id{item.get_id()};
			is_joinable.emplace(id,false);
			thr.emplace(id,move(item));
			waitingQue.emplace(&thr[id]);
		}
	}

	CThreadPool::thread_id CThreadPool::Impl::add(function<void()> &&func)
	{
		auto temp{waitingQue.wait_and_pop()};
		is_joinable[temp->get_id()]=true;
		temp->assign(make_unique<CThreadPoolItemExecutorJoin>(make_unique<CThreadPoolCommunJoin>(temp,join_anyList,waitingQue),move(func)));
		return temp->get_id();
	}

	void CThreadPool::Impl::add_and_detach(function<void()> &&func)
	{
		auto temp{waitingQue.wait_and_pop()};
		is_joinable[temp->get_id()]=false;
		temp->assign(make_unique<CThreadPoolItemExecutorDetach>(make_unique<CThreadPoolCommunDetach>(temp,waitingQue),move(func)));
	}

	void CThreadPool::Impl::join_all()
	{
		lock_guard<mutex> lock{join_all_mut};
		for(auto &val:thr)
			if(joinable(val.first))
				thr[val.first].wait();
	}

	bool CThreadPool::Impl::joinable(const thread::id id) const noexcept
	{
		if(is_joinable.at(id))
			return thr.at(id).is_running();
		return false;
	}

	void CThreadPool::Impl::wait_until_all_available()
	{
		vector<decltype(waitingQue)::value_type> vec;
		vec.reserve(thr.size());
		lock_guard<mutex> lock{wait_until_all_available_mut};
		while(vec.size()!=vec.capacity())
			vec.emplace_back(waitingQue.wait_and_pop());
		for(auto &val:vec)
			waitingQue.emplace(move(val));
	}

	CThreadPool::thread_id CThreadPool::add_(std::function<void()> &&func)
	{
		return impl_.get().add(move(func));
	}

	void CThreadPool::add_and_detach_(std::function<void()> &&func)
	{
		impl_.get().add_and_detach(move(func));
	}

	CThreadPool::CThreadPool(const size_t size)
		:impl_{size}{}
	
	size_t CThreadPool::available() const noexcept
	{
		return impl_.get().waitingQue.size();
	}

	size_t CThreadPool::count() const noexcept
	{
		return impl_.get().thr.size();
	}

	void CThreadPool::join(const thread_id id)
	{
		impl_.get().thr[id].wait();
	}

	void CThreadPool::join_all()
	{
		impl_.get().join_all();
	}

	CThreadPool::thread_id CThreadPool::join_any()
	{
		auto temp{impl_.get().join_anyList.wait_and_pop()};
		temp->wait();
		return temp->get_id();
	}

	bool CThreadPool::joinable(const thread_id id) const noexcept
	{
		return impl_.get().joinable(id);
	}

	void CThreadPool::wait_until_all_available() const
	{
		impl_.get().wait_until_all_available();
	}

	CThreadPool::~CThreadPool(){}
}