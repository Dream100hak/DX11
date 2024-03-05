#include "pch.h"
#include "JobQueue.h"

/*--------------
	JobQueue
---------------*/

void JobQueue::Push(shared_ptr<Job> job, bool pushOnly)
{
	_jobs.push(job);
}

void JobQueue::Execute()
{
	while (_jobs.empty() == false)
	{
		shared_ptr<Job> job = _jobs.front(); 
		_jobs.pop();
		job->Execute();
	}
}
