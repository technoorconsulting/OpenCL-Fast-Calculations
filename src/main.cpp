/*
 * Copyright 1993-2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and 
 * proprietary rights in and to this software and related documentation. 
 * Any use, reproduction, disclosure, or distribution of this software 
 * and related documentation without an express license agreement from
 * NVIDIA Corporation is strictly prohibited.
 *
 * Please refer to the applicable NVIDIA end user license agreement (EULA) 
 * associated with this source code for terms and conditions that govern 
 * your use of this NVIDIA software.
 * 
 */

// standard utilities and systems includes
#include <oclUtils.h>
#include "oclBlackScholes_common.h"
#include <iostream>

#define GPU_PROFILING
////////////////////////////////////////////////////////////////////////////////
// Helper functions
////////////////////////////////////////////////////////////////////////////////
template<class T, class C> void foo(T a, C b){};
template<class C, class T> void foo2(T a, C b){};


double executionTime(cl_event &event){
    cl_ulong start, end;

    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);

    return (double)1.0e-9 * (end - start); // convert nanoseconds to seconds on return
}

////////////////////////////////////////////////////////////////////////////////
// Random float helper
////////////////////////////////////////////////////////////////////////////////
float randFloat(float low, float high){
    float t = (float)rand() / (float)RAND_MAX;
    return (1.0f - t) * low + t * high;
}
struct QP{

double PCFreq; 
long long CounterStart; 
 
void StartCounter() 
{ 
	CounterStart = 0;
	PCFreq = 0.0;
    LARGE_INTEGER li; 
    if(!QueryPerformanceFrequency(&li)) 
        std::cout << "QueryPerformanceFrequency failed!\n"; 
 
    PCFreq = double(li.QuadPart)/1000.0; 
 
    QueryPerformanceCounter(&li); 
    CounterStart = li.QuadPart; 
} 
double GetCounter() 
{ 
    LARGE_INTEGER li; 
    QueryPerformanceCounter(&li); 
    return double(li.QuadPart-CounterStart)/PCFreq; 
}

};
 
 struct C_T:public QP
 {};

////////////////////////////////////////////////////////////////////////////////
// Main program
////////////////////////////////////////////////////////////////////////////////
int main(int argc, const char **argv){
    cl_platform_id cpPlatform;       //OpenCL platform
    cl_device_id cdDevice;           //OpenCL devices
    cl_context      cxGPUContext;    //OpenCL context
    cl_command_queue cqCommandQueue; //OpenCL command que
    cl_mem                           //OpenCL memory buffer objects
        d_Call,
        d_Put,
        d_S,
        d_X,
        d_T;

    cl_int ciErrNum;

    float
        *h_CallCPU,
        *h_PutCPU,
        *h_CallGPU,
        *h_PutGPU,
        *h_S,
        *h_X,
        *h_T;

    const unsigned int   optionCount = 4000000;
    const float                    R = 0.02f;
    const float                    V = 0.30f;
	QP & qp = dynamic_cast<QP&>( *(new C_T));
	std::string test("hello");
	char a = test[1];
    // set logfile name and start logs
    shrSetLogFileName ("oclBlackScholes.txt");
    shrLog("%s Starting...\n\n", argv[0]); 

    shrLog("Allocating and initializing host memory...\n");
        h_CallCPU = (float *)malloc(optionCount * sizeof(float));
        h_PutCPU  = (float *)malloc(optionCount * sizeof(float));
        h_CallGPU = (float *)malloc(optionCount * sizeof(float));
        h_PutGPU  = (float *)malloc(optionCount * sizeof(float));
        h_S       = (float *)malloc(optionCount * sizeof(float));
        h_X       = (float *)malloc(optionCount * sizeof(float));
        h_T       = (float *)malloc(optionCount * sizeof(float));

        srand(2009);
        for(unsigned int i = 0; i < optionCount; i++){
            h_CallCPU[i] = -1.0f;
            h_PutCPU[i]  = -1.0f;
            h_S[i]       = randFloat(5.0f, 30.0f);
            h_X[i]       = randFloat(1.0f, 100.0f);
            h_T[i]       = randFloat(0.25f, 10.0f);
        }

    shrLog("Initializing OpenCL...\n");
        // Get the NVIDIA platform
        ciErrNum = oclGetPlatformID(&cpPlatform);
        oclCheckError(ciErrNum, CL_SUCCESS);

        // Get a GPU device
        ciErrNum = clGetDeviceIDs(cpPlatform, CL_DEVICE_TYPE_GPU, 1, &cdDevice, NULL);
        oclCheckError(ciErrNum, CL_SUCCESS);

        // Create the context
        cxGPUContext = clCreateContext(0, 1, &cdDevice, NULL, NULL, &ciErrNum);
        oclCheckError(ciErrNum, CL_SUCCESS);

        //Create a command-queue
        cqCommandQueue = clCreateCommandQueue(cxGPUContext, cdDevice, CL_QUEUE_PROFILING_ENABLE, &ciErrNum);
        oclCheckError(ciErrNum, CL_SUCCESS);

    shrLog("Creating OpenCL memory objects...\n");
        d_Call = clCreateBuffer(cxGPUContext, CL_MEM_READ_WRITE, optionCount * sizeof(float), NULL, &ciErrNum);
        oclCheckError(ciErrNum, CL_SUCCESS);
        d_Put  = clCreateBuffer(cxGPUContext, CL_MEM_READ_WRITE, optionCount * sizeof(float), NULL, &ciErrNum);
        oclCheckError(ciErrNum, CL_SUCCESS);
        d_S    = clCreateBuffer(cxGPUContext, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, optionCount * sizeof(float), h_S, &ciErrNum);
        oclCheckError(ciErrNum, CL_SUCCESS);
        d_X    = clCreateBuffer(cxGPUContext, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, optionCount * sizeof(float), h_X, &ciErrNum);
        oclCheckError(ciErrNum, CL_SUCCESS);
        d_T    = clCreateBuffer(cxGPUContext, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, optionCount * sizeof(float), h_T, &ciErrNum);
        oclCheckError(ciErrNum, CL_SUCCESS);

    shrLog("Starting up BlackScholes...\n");
        initBlackScholes(cxGPUContext, cqCommandQueue, argv);

    shrLog("Running OpenCL BlackScholes...\n\n");
        //Just a single run or a warmup iteration
        BlackScholes(
            NULL,
            d_Call,
            d_Put,
            d_S,
            d_X,
            d_T,
            R,
            V,
            optionCount
        );

#ifdef GPU_PROFILING
    int numIterations = 2000;
    cl_event startMark, endMark;
    ciErrNum = clEnqueueMarker(cqCommandQueue, &startMark);
    ciErrNum |= clFinish(cqCommandQueue);
    shrCheckError(ciErrNum, CL_SUCCESS);
    shrDeltaT(0);

    for(int i = 0; i < numIterations; i++){
		BlackScholes(
            cqCommandQueue,
            d_Call,
            d_Put,
            d_S,
            d_X,
            d_T,
            R,
            V,
            optionCount
        );
    }

    ciErrNum  = clEnqueueMarker(cqCommandQueue, &endMark);
    ciErrNum |= clFinish(cqCommandQueue);
    shrCheckError(ciErrNum, CL_SUCCESS);

    //Calculate performance metrics by wallclock time
    double gpuTime = shrDeltaT(0) / numIterations;
    shrLogEx(LOGBOTH | MASTER, 0, "oclBlackScholes, Throughput = %.4f GOptions/s, Time = %.5f s, Size = %u options, NumDevsUsed = %i, Workgroup = %u\n", 
        (double)(2.0 * optionCount * 1.0e-9)/gpuTime, gpuTime, (2 * optionCount), 1, 0);

    //Get profiling info
    cl_ulong startTime = 0, endTime = 0;
    ciErrNum  = clGetEventProfilingInfo(startMark, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &startTime, NULL);
    ciErrNum |= clGetEventProfilingInfo(endMark, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &endTime, NULL);
    shrCheckError(ciErrNum, CL_SUCCESS);
    shrLog("\nOpenCL time: %.5f s\n\n", 1.0e-9 * ((double)endTime - (double)startTime) / (double)numIterations);
#endif

    shrLog("\nReading back OpenCL BlackScholes results...\n");
        ciErrNum = clEnqueueReadBuffer(cqCommandQueue, d_Call, CL_TRUE, 0, optionCount * sizeof(float), h_CallGPU, 0, NULL, NULL);
        oclCheckError(ciErrNum, CL_SUCCESS);
        ciErrNum = clEnqueueReadBuffer(cqCommandQueue, d_Put, CL_TRUE, 0, optionCount * sizeof(float), h_PutGPU, 0, NULL, NULL);
        oclCheckError(ciErrNum, CL_SUCCESS);

    shrLog("Comparing against Host/C++ computation...\n");

//	    QP qp;
		qp.StartCounter();
		numIterations=20;
		for(int i = 0; i < numIterations; i++){
        BlackScholesCPU(h_CallCPU, h_PutCPU, h_S, h_X, h_T, R, V, optionCount);
		}
		shrLog("\CPU time: %.5f s\n\n", qp.GetCounter()/numIterations);


        double deltaCall = 0, deltaPut = 0, sumCall = 0, sumPut = 0;
        double L1call, L1put;
        for(unsigned int i = 0; i < optionCount; i++)
        {
            sumCall += fabs(h_CallCPU[i]);
            sumPut  += fabs(h_PutCPU[i]);
            deltaCall += fabs(h_CallCPU[i] - h_CallGPU[i]);
            deltaPut  += fabs(h_PutCPU[i] - h_PutGPU[i]);
        }
        L1call = deltaCall / sumCall; 
        L1put = deltaPut / sumPut;
        shrLog("Relative L1 (call, put) = (%.3e, %.3e)\n\n", L1call, L1put);

    shrLog("%s\n\n", ((L1call < 1E-6) && (L1put < 1E-6)) ? "PASSED" : "FAILED");

    shrLog("Shutting down...\n");
        closeBlackScholes();
        ciErrNum  = clReleaseMemObject(d_T);
        ciErrNum |= clReleaseMemObject(d_X);
        ciErrNum |= clReleaseMemObject(d_S);
        ciErrNum |= clReleaseMemObject(d_Put);
        ciErrNum |= clReleaseMemObject(d_Call);
        ciErrNum |= clReleaseCommandQueue(cqCommandQueue);
        ciErrNum |= clReleaseContext(cxGPUContext);
        oclCheckError(ciErrNum, CL_SUCCESS);

        free(h_T);
        free(h_X);
        free(h_S);
        free(h_PutGPU);
        free(h_CallGPU);
        free(h_PutCPU);
        free(h_CallCPU);

        shrEXIT(argc, argv);
}
