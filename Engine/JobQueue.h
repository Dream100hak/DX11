#pragma once
#include "Job.h"

/*--------------
	JobQueue
---------------*/

class JobQueue : public enable_shared_from_this<JobQueue>
{
public:
	void DoPush(CallbackType&& callback)
	{
		Push(make_shared<Job>(std::move(callback)));
	}

	template<typename T, typename Ret, typename... Args>
	void DoPush(Ret(T::* memFunc)(Args...), Args... args)
	{
		shared_ptr<T> owner = static_pointer_cast<T>(shared_from_this());
		Push(make_shared<Job>(owner, memFunc, std::forward<Args>(args)...));
	}

	void	Execute();
	void	ClearJobs() { _jobs = queue<shared_ptr<Job>>();  }

private:
	void	Push(shared_ptr<Job> job, bool pushOnly = false);
	

protected:
	queue<shared_ptr<Job>> _jobs;
};

