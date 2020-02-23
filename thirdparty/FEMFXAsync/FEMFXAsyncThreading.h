/*
MIT License

Copyright (c) 2019 Advanced Micro Devices, Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

//---------------------------------------------------------------------------------------
// Support for asynchronous approach for task scheduling, where dispatched tasks
// track progress and issue follow-up tasks after work complete.
//---------------------------------------------------------------------------------------

#pragma once

#include "FEMFXTaskSystemInterface.h"
#include "stdint.h"
#include <atomic>

namespace AMD
{
    // Creates a task function and wrapper function to run task inside FmExecuteTask
#define FM_WRAPPED_TASK_FUNC(taskFunc) \
    void taskFunc(void* inTaskData, int32_t inTaskBeginIndex, int32_t inTaskEndIndex); \
    void taskFunc##Wrapped(void* inTaskData, int32_t inTaskBeginIndex, int32_t inTaskEndIndex) { FmExecuteTask(taskFunc, inTaskData, inTaskBeginIndex, inTaskEndIndex); } \
    void taskFunc(void* inTaskData, int32_t inTaskBeginIndex, int32_t inTaskEndIndex)

#define FM_WRAPPED_TASK_DECLARATION(taskFunc) \
    void taskFunc(void* inTaskData, int32_t inTaskBeginIndex, int32_t inTaskEndIndex); \
    void taskFunc##Wrapped(void* inTaskData, int32_t inTaskBeginIndex, int32_t inTaskEndIndex);

#define FM_TASK_AND_WRAPPED_TASK_ARGS(taskFunc) taskFunc, taskFunc##Wrapped

    struct FmTask
    {
        FmTaskFuncCallback func;
        void*              data;
        int32_t            beginIndex;
        int32_t            endIndex;

        FmTask() : func(NULL), data(NULL), beginIndex(0), endIndex(0) {}
    };

    // Async tasks need to be executed using this function, which starts a loop that will pick up subsequent tasks set by FmSetNextTask()
    void FmExecuteTask(FmTaskFuncCallback taskFunc, void* taskData, int32_t taskBeginIndex, int32_t taskEndIndex);

    // Set next task for thread to execute in FmExecuteTask() loop
    void FmSetNextTask(FmTaskFuncCallback taskFunc, void* taskData, int32_t taskBeginIndex, int32_t taskEndIndex);
    void FmSetNextTask(const FmTask& inTask);

    // Holds an atomic count of incomplete tasks, and the follow-up task to execute when all tasks are complete.
    class FmAsyncTasksProgress
    {
        std::atomic_uint32_t nextIndex;                // Atomic index used to start processing work items in sorted order
        std::atomic_uint32_t numTasksIncomplete;       // Atomic count of remaining tasks to detect completion
        FmTask followTask;

    public:

        FmAsyncTasksProgress()
        {
            nextIndex.store(0);
            numTasksIncomplete.store(0);
            followTask.func = NULL;
            followTask.data = NULL;
            followTask.beginIndex = 0;
            followTask.endIndex = 1;
        }

        void Init(uint32_t inNumTasksToRun, FmTaskFuncCallback inFollowTask, void* inFollowTaskData)
        {            
            nextIndex.store(0);
            numTasksIncomplete.store(inNumTasksToRun);
            followTask.func = inFollowTask;
            followTask.data = inFollowTaskData;
        }

        void Init(uint32_t inNumTasksToRun, FmTaskFuncCallback inFollowTask, void* inFollowTaskData, int32_t inFollowTaskBeginIndex, int32_t inFollowTaskEndIndex)
        {
            nextIndex.store(0);
            numTasksIncomplete.store(inNumTasksToRun);
            followTask.func = inFollowTask;
            followTask.data = inFollowTaskData;
            followTask.beginIndex = inFollowTaskBeginIndex;
            followTask.endIndex = inFollowTaskEndIndex;
        }

        void ResetNextIndex()
        {
            nextIndex.store(0);
        }

        uint32_t GetNextIndex()
        {
            return nextIndex++ - 1;
        }

        // Increment number of incomplete tasks.
        void TaskIsStarting()
        {
            nextIndex++;
        }

        // Add to number of incomplete tasks.
        void TasksAreStarting(uint32_t numTasks)
        {
            numTasksIncomplete += numTasks;
        }

        // Decrement number of incomplete tasks.
        // If this is last task, delete task data, and run follow task.
        // This FmAsyncTasksProgress can belong to task data.
        // 
        // NOTE: This sets the global next task function rather than making a call, so
        // must be executed within a FmTaskFuncLoop() and no other async calls or 
        // waits can occur before returning to the loop.
        template<class T>
        bool TasksAreFinished(uint32_t numFinishedTasks, T* taskDataToDelete)
        {
#if FM_ASYNC_THREADING
            numTasksIncomplete -= numFinishedTasks;
            uint32_t numTasks = numTasksIncomplete;

            if (numTasks == 0)
            {
                if (followTask.func)
                {
                    FmSetNextTask(followTask);
                }

                return true;
            }
            else
            {
                return false;
            }
#else
            uint32_t numTasks = FmAtomicSub(&numTasksIncomplete.val, numFinishedTasks);

            if (numTasks == 0)
            {
                return true;
            }
            else
            {
                return false;
            }
#endif
        }

        template<class T>
        bool TaskIsFinished(T* taskDataToDelete)
        {
            return TasksAreFinished(1, taskDataToDelete);
        }

        bool TaskIsFinished()
        {
            return TaskIsFinished<void>(NULL);
        }

        bool TasksAreFinished(uint32_t numTasks)
        {
            return TasksAreFinished<void>(numTasks, NULL);
        }
    };

    // Task data base class
    class FmAsyncTaskData
    {
    public:

        FmAsyncTasksProgress progress;        // Progress of a parallel operation plus a task to follow
        FmTask               followTask;      // Task to call following scope of this task data
        FmAsyncTaskData*     parentTaskData;  // Pointer to parent task data for nested parallel operations

        FmAsyncTaskData() : parentTaskData(NULL) { }

        virtual ~FmAsyncTaskData() {}
    };
}
