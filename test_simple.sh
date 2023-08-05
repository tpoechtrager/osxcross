#!/usr/bin/env bash

trap "exit 0" INT EXIT TERM HUP PIPE QUIT ILL KILL ABRT

################################################################################
# OS X Cross Build Test Some Simple, Perhaps Trivial C, C++, Fortran Programs:
################################################################################
# This script generates single source file programs in varioius languages and
#  compiels them using the OS X Cross toolchain.
################################################################################
# NOTE: The OS X Cross stage directory target/bin is assumed to be at the
#  beginning of the PATH.
#
# The following command must be able to locate the toolchain compilers:
#
#     xcrun -f cc
################################################################################
# The following environment variables effect the operation of this script:
#
#  # Set the test architecture to use:
#  OSXCROSS_TEST_ARCH=aarch64|arm64
#  OSXCROSS_TEST_ARCH=x86_64|x86_64h|i386
#  OSXCROSS_TEST_ARCH=powerpc|powerpc64
################################################################################

################################################################################
# Determine the host prefix and compiler tools:
################################################################################

OSXCROSS_TEST_ARCH="${OSXCROSS_TEST_ARCH:-x86_64}"
OSXCROSS_TEST_OSVERSION="unknown"
OSXCROSS_TEST_XCRUNCC="$(xcrun -f cc 2>/dev/null)"
OSXCROSS_TEST_XCRUNCC_VERSION_REGEX1=".*darwin([0-9])[-]cc$"
OSXCROSS_TEST_XCRUNCC_VERSION_REGEX2=".*darwin([0-9]{2})[-]cc$"
OSXCROSS_TEST_XCRUNCC_VERSION_REGEX3=".*darwin([0-9]{2}[.][0-9])[-]cc$"
OSXCROSS_TEST_XCRUNCC_VERSION_REGEX4=".*darwin([0-9]{2}[.][0-9]{2})[-]cc$"
if [[ $OSXCROSS_TEST_XCRUNCC =~ $OSXCROSS_TEST_XCRUNCC_VERSION_REGEX1 ]]
then
   OSXCROSS_TEST_OSVERSION="${BASH_REMATCH[1]}"
elif [[ $OSXCROSS_TEST_XCRUNCC =~ $OSXCROSS_TEST_XCRUNCC_VERSION_REGEX2 ]]
then
   OSXCROSS_TEST_OSVERSION="${BASH_REMATCH[1]}"
elif [[ $OSXCROSS_TEST_XCRUNCC =~ $OSXCROSS_TEST_XCRUNCC_VERSION_REGEX3 ]]
then
   OSXCROSS_TEST_OSVERSION="${BASH_REMATCH[1]}"
elif [[ $OSXCROSS_TEST_XCRUNCC =~ $OSXCROSS_TEST_XCRUNCC_VERSION_REGEX4 ]]
then
   OSXCROSS_TEST_OSVERSION="${BASH_REMATCH[1]}"
else
   echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
      "Cannot Determine Toolchain OSVersion." 1>&2
   exit 1
fi

OSXCROSS_TEST_HOST_PREFIX=\
"${OSXCROSS_TEST_ARCH}-apple-darwin${OSXCROSS_TEST_OSVERSION}"
OSXCROSS_TEST_TOOLCHAIN_CC="${OSXCROSS_TEST_HOST_PREFIX}-gcc"
OSXCROSS_TEST_TOOLCHAIN_CXX="${OSXCROSS_TEST_HOST_PREFIX}-g++"
OSXCROSS_TEST_TOOLCHAIN_FORTRAN="${OSXCROSS_TEST_HOST_PREFIX}-gfortran"
OSXCROSS_TEST_TOOLCHAIN_AR="${OSXCROSS_TEST_HOST_PREFIX}-ar"
OSXCROSS_TEST_TOOLCHAIN_RANLIB="${OSXCROSS_TEST_HOST_PREFIX}-ranlib"
echo
echo "======================================================================"
echo " OS X Cross Test:"
echo "======================================================================"
echo " OSXCROSS_TEST_ARCH=${OSXCROSS_TEST_ARCH}"
echo " OSXCROSS_TEST_HOST_PREFIX=${OSXCROSS_TEST_HOST_PREFIX}"
echo " OSXCROSS_TEST_TOOLCHAIN_CC=${OSXCROSS_TEST_TOOLCHAIN_CC}"
echo "======================================================================"
OSXCROSS_TEST_TOOLCHAIN_CC_RUN="$(${OSXCROSS_TEST_TOOLCHAIN_CC} --version 2>&1)"
if [ "${?}x" = "0x" ]
then
   echo "${OSXCROSS_TEST_TOOLCHAIN_CC_RUN}"
else
   echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
      "Failure running '${OSXCROSS_TEST_TOOLCHAIN_CC} --version'." 1>&2
   echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
      "${OSXCROSS_TEST_TOOLCHAIN_CC_RUN}" 1>&2
   exit 1
fi

################################################################################
# Create the test and stage directories:
################################################################################

OSXCROSS_TEST_DIR="$(pwd)/test/${OSXCROSS_TEST_ARCH}"
mkdir -p "${OSXCROSS_TEST_DIR}"
if [ ! -d "${OSXCROSS_TEST_DIR}" ]
then
   echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
      "Cannot create Test Directory '${OSXCROSS_TEST_DIR}'." 1>&2
   exit 1
fi

echo
echo "======================================================================"
echo " OSXCROSS_TEST_DIR=${OSXCROSS_TEST_DIR}"
echo "======================================================================"

################################################################################
# FFPROG01: Fortran77 Helloworld Program:
################################################################################

echo
echo "======================================================================"
echo " FFPROG01: Fortran77 Helloworld Program:"
echo "======================================================================"

FFPROG01_PREFIX="FFPROG01_f77_hello_world"
FFPROG01_SOURCE="${FFPROG01_PREFIX}.f"
FFPROG01_BINARY="${FFPROG01_PREFIX}.bin.${OSXCROSS_TEST_ARCH}"
cat <<EOF >"${OSXCROSS_TEST_DIR}/${FFPROG01_SOURCE}"
c FFPROG01: Fortran77 Helloworld Program:
      program hello_world
      implicit none
c
      call hello
      call hello
c
      end
c
      subroutine hello
      implicit none
      character*32 text
c
      text = 'Hello World!'
      write (*,*) text
c
      end
EOF

(
   cd "${OSXCROSS_TEST_DIR}/" \
   && "${OSXCROSS_TEST_TOOLCHAIN_FORTRAN}" \
      -O6 -Wall -g \
      -static-libgcc \
      ./"${FFPROG01_SOURCE}" \
      -o ./"${FFPROG01_BINARY}" \
   && file ./"${FFPROG01_BINARY}" \
   && xcrun otool -arch all -hvL ./"${FFPROG01_BINARY}" \
)

################################################################################
# FFPROG02: Fortran90 Helloworld Program:
################################################################################

echo
echo "======================================================================"
echo " FFPROG02: Fortran90 Helloworld Program:"
echo "======================================================================"

FFPROG02_PREFIX="FFPROG02_f90_hello_world"
FFPROG02_SOURCE="${FFPROG02_PREFIX}.f90"
FFPROG02_BINARY="${FFPROG02_PREFIX}.bin.${OSXCROSS_TEST_ARCH}"
cat <<EOF >"${OSXCROSS_TEST_DIR}/${FFPROG02_SOURCE}"
! FFPROG02: Fortran90 Helloworld Program:
program main
  implicit none
  write ( *, '(a)' ) '  Hello, world!'
  stop
end
EOF

(
   cd "${OSXCROSS_TEST_DIR}/" \
   && "${OSXCROSS_TEST_TOOLCHAIN_FORTRAN}" \
      -O6 -Wall -g \
      -static-libgcc \
      ./"${FFPROG02_SOURCE}" \
      -o ./"${FFPROG02_BINARY}" \
   && file ./"${FFPROG02_BINARY}" \
   && xcrun otool -arch all -hvL ./"${FFPROG02_BINARY}" \
)

################################################################################
# CCPROG01: C89 Helloworld Program:
################################################################################

echo
echo "======================================================================"
echo " CCPROG01: C89 Helloworld Program:"
echo "======================================================================"

CCPROG01_PREFIX="CCPROG01_c89_hello_world"
CCPROG01_SOURCE="${CCPROG01_PREFIX}.c"
CCPROG01_BINARY="${CCPROG01_PREFIX}.bin.${OSXCROSS_TEST_ARCH}"
cat <<EOF >"${OSXCROSS_TEST_DIR}/${CCPROG01_SOURCE}"
// CCPROG01: C89 Helloworld Program:
#include <stdlib.h>
#include <stdio.h>

int main(void)
{
   printf("Hello World!\n");

   return EXIT_SUCCESS;
}
EOF

(
   cd "${OSXCROSS_TEST_DIR}/" \
   && "${OSXCROSS_TEST_TOOLCHAIN_CC}" \
      -O6 -Wall -g \
      -static-libgcc \
      ./"${CCPROG01_SOURCE}" \
      -o ./"${CCPROG01_BINARY}" \
   && file ./"${CCPROG01_BINARY}" \
   && xcrun otool -arch all -hvL ./"${CCPROG01_BINARY}" \
)

################################################################################
# CCPROG02: C11 Program Using PThreads:
################################################################################

echo
echo "======================================================================"
echo " CCPROG02: C11 Program Using PThreads:"
echo "======================================================================"

CCPROG02_PREFIX="CCPROG02_c11_phtreads"
CCPROG02_SOURCE="${CCPROG02_PREFIX}.c"
CCPROG02_BINARY="${CCPROG02_PREFIX}.bin.${OSXCROSS_TEST_ARCH}"
cat <<EOF >"${OSXCROSS_TEST_DIR}/${CCPROG02_SOURCE}"
// CCPROG02: C11 Program Using PThreads:
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

static bool threadIsRunning = false;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static void * ThreadFunction(void * arg)
{
   pthread_mutex_lock(&mutex);
   threadIsRunning = true;
   pthread_cond_signal(&cond);
   pthread_mutex_unlock(&mutex);
   printf(
         "In ThreadFunction(): arg='%s'.\n",
         (arg != NULL) ? (const char *)arg : "");
   return NULL;
}

int main(void)
{
   const char * hwString = "Hello World!";
   int ptResult;
   pthread_t pThread;

   printf("In Main().\n");

   pthread_mutex_lock(&mutex);

   ptResult = pthread_create(&pThread, NULL, &ThreadFunction, (void *)hwString);
   if (ptResult != 0)
   {
      errno = ptResult;
      perror("Failure: Creating Thread.");
      exit(EXIT_FAILURE);
   }

   while (threadIsRunning == false)
   {
      pthread_cond_wait(&cond, &mutex);
   }

   pthread_mutex_unlock(&mutex);

   ptResult = pthread_join(pThread, NULL);
   if (ptResult != 0)
   {
      errno = ptResult;
      perror("Failure: Joining Thread.");
      exit(EXIT_FAILURE);
   }

   printf("Back In Main().\n");

   return EXIT_SUCCESS;
}
EOF

(
   cd "${OSXCROSS_TEST_DIR}/" \
   && "${OSXCROSS_TEST_TOOLCHAIN_CC}" \
      -O6 -Wall -g \
      -static-libgcc \
      -std=c11 \
      ./"${CCPROG02_SOURCE}" \
      -o ./"${CCPROG02_BINARY}" \
   && file ./"${CCPROG02_BINARY}" \
   && xcrun otool -arch all -hvL ./"${CCPROG02_BINARY}" \
)

################################################################################
# CXPROG01: C++03 Helloworld Program:
################################################################################

echo
echo "======================================================================"
echo " CXPROG01: C++03 Helloworld Program:"
echo "======================================================================"

CXPROG01_PREFIX="CXPROG01_c++03_hello_world"
CXPROG01_SOURCE="${CXPROG01_PREFIX}.cpp"
CXPROG01_BINARY="${CXPROG01_PREFIX}.bin.${OSXCROSS_TEST_ARCH}"
cat <<EOF >"${OSXCROSS_TEST_DIR}/${CXPROG01_SOURCE}"
// CXPROG01: C++03 Helloworld Program:
#include <cstdlib>
#include <iostream>
#include <string>

int main()
{
   const ::std::string hwString("Hello World!");

   ::std::cout << hwString << ::std::endl;

   return EXIT_SUCCESS;
}
EOF

(
   cd "${OSXCROSS_TEST_DIR}/" \
   && "${OSXCROSS_TEST_TOOLCHAIN_CXX}" \
      -O6 -Wall -g \
      -static-libgcc \
      -std=c++03 \
      ./"${CXPROG01_SOURCE}" \
      -o ./"${CXPROG01_BINARY}" \
   && file ./"${CXPROG01_BINARY}" \
   && xcrun otool -arch all -hvL ./"${CXPROG01_BINARY}" \
)

################################################################################
# CXPROG02: C++11 Program Using ::std::thread:
################################################################################

echo
echo "======================================================================"
echo " CXPROG02: C++11 Program Using ::std::thread:"
echo "======================================================================"

CXPROG02_PREFIX="CXPROG02_c++11_std_thread"
CXPROG02_SOURCE="${CXPROG02_PREFIX}.cpp"
CXPROG02_BINARY="${CXPROG02_PREFIX}.bin.${OSXCROSS_TEST_ARCH}"
cat <<EOF >"${OSXCROSS_TEST_DIR}/${CXPROG02_SOURCE}"
// CXPROG02: C++11 Program Using ::std::thread:
#include <cstdlib>
#include <iostream>
#include <thread>
#include <condition_variable>
#include <mutex>

static bool threadIsRunning(false);
static ::std::condition_variable cv;
static ::std::mutex cvm;

static void ThreadFunction()
{
   cvm.lock();
   threadIsRunning = true;
   cv.notify_one();
   cvm.unlock();

   ::std::cout << "In ThreadFunction():" << ::std::endl;
}

int main()
{
   ::std::unique_lock< ::std::mutex> cvlk(cvm);

   ::std::cout << "In Main():" << ::std::endl;

   ::std::thread th(&ThreadFunction);

   while (!threadIsRunning)
   {
      cv.wait(cvlk);
   }

   cvlk.unlock();

   th.join();

   ::std::cout << "Back In Main():" << ::std::endl;

   return EXIT_SUCCESS;
}
EOF

(
   cd "${OSXCROSS_TEST_DIR}/" \
   && "${OSXCROSS_TEST_TOOLCHAIN_CXX}" \
      -O6 -Wall -g \
      -static-libgcc \
      -std=c++11 \
      ./"${CXPROG02_SOURCE}" \
      -o ./"${CXPROG02_BINARY}" \
   && file ./"${CXPROG02_BINARY}" \
   && xcrun otool -arch all -hvL ./"${CXPROG02_BINARY}" \
)

################################################################################
# CXPROG03: C++11 Program Using Lamdas:
################################################################################

echo
echo "======================================================================"
echo " CXPROG03: C++11 Program Using Lamdas:"
echo "======================================================================"

CXPROG03_PREFIX="CXPROG03_c++11_lamdas"
CXPROG03_SOURCE="${CXPROG03_PREFIX}.cpp"
CXPROG03_BINARY="${CXPROG03_PREFIX}.bin.${OSXCROSS_TEST_ARCH}"
cat <<EOF >"${OSXCROSS_TEST_DIR}/${CXPROG03_SOURCE}"
// CXPROG03: C++11 Program Using Lamdas:
#include <cstdlib>
#include <iostream>

int main()
{
   ::std::cout << [](int m, int n) { return m + n;} (2,4) << ::std::endl;

   return EXIT_SUCCESS;
}
EOF

(
   cd "${OSXCROSS_TEST_DIR}/" \
   && "${OSXCROSS_TEST_TOOLCHAIN_CXX}" \
      -O6 -Wall -g \
      -static-libgcc \
      -std=c++11 \
      ./"${CXPROG03_SOURCE}" \
      -o ./"${CXPROG03_BINARY}" \
   && file ./"${CXPROG03_BINARY}" \
   && xcrun otool -arch all -hvL ./"${CXPROG03_BINARY}" \
)

################################################################################
# CXPROG04: C++14 Program Using Generic Lamdas:
################################################################################

echo
echo "======================================================================"
echo " CXPROG04: C++14 Program Using Generic Lamdas:"
echo "======================================================================"

CXPROG04_PREFIX="CXPROG04_c++14_generic_lamdas"
CXPROG04_SOURCE="${CXPROG04_PREFIX}.cpp"
CXPROG04_BINARY="${CXPROG04_PREFIX}.bin.${OSXCROSS_TEST_ARCH}"
cat <<EOF >"${OSXCROSS_TEST_DIR}/${CXPROG04_SOURCE}"
// CXPROG04: C++14 Program Using Generic Lamdas:
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <numeric>

int main()
{
  ::std::vector<int> ivec = { 1, 2, 3, 4};
  ::std::vector<std::string> svec = { "red",
                                    "green",
                                    "blue" };
  auto adder  = [](auto op1, auto op2){ return op1 + op2; };
  ::std::cout << "int result : "
            << ::std::accumulate(ivec.begin(),
                               ivec.end(),
                               0,
                               adder )
            << "\n";
  ::std::cout << "string result : "
            << ::std::accumulate(svec.begin(),
                               svec.end(),
                               ::std::string(""),
                               adder )
            << "\n";

   return EXIT_SUCCESS;
}
EOF

(
   cd "${OSXCROSS_TEST_DIR}/" \
   && "${OSXCROSS_TEST_TOOLCHAIN_CXX}" \
      -O6 -Wall -g \
      -static-libgcc \
      -std=c++14 \
      ./"${CXPROG04_SOURCE}" \
      -o ./"${CXPROG04_BINARY}" \
   && file ./"${CXPROG04_BINARY}" \
   && xcrun otool -arch all -hvL ./"${CXPROG04_BINARY}" \
)

################################################################################
# CXPROG05: C++14 Program Using ::std::shared_timed_mutex:
################################################################################

echo
echo "======================================================================"
echo " CXPROG05: C++14 Program Using ::std::shared_timed_mutex:"
echo "======================================================================"

CXPROG05_PREFIX="CXPROG05_c++14_using_std_shared_timed_mutex"
CXPROG05_SOURCE="${CXPROG05_PREFIX}.cpp"
CXPROG05_BINARY="${CXPROG05_PREFIX}.bin.${OSXCROSS_TEST_ARCH}"
cat <<EOF >"${OSXCROSS_TEST_DIR}/${CXPROG05_SOURCE}"
// CXPROG05: C++14 Program Using ::std::shared_timed_mutex:
#include <cstdlib>
#include <iostream>
#include <vector>
#include <list>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <thread>

#define MAIN_WAIT_MILLISECONDS 220

static void useSTLSharedTimedMutex()
{
   std::shared_timed_mutex shared_mtx_lock;

   std::vector<std::thread> readThreads;
   std::vector<std::thread> writeThreads;

   std::list<int> data = { 0 };
   volatile bool exit = false;

   std::atomic<int> readProcessedCnt(0);
   std::atomic<int> writeProcessedCnt(0);

   for (unsigned int i = 0; i < std::thread::hardware_concurrency(); i++)
   {

       readThreads.push_back(std::thread([&data, &exit, &shared_mtx_lock, &readProcessedCnt]() {
           std::list<int> mydata;
           int localProcessCnt = 0;

           while (true)
           {
               shared_mtx_lock.lock_shared();

               mydata.push_back(data.back());
               ++localProcessCnt;

               shared_mtx_lock.unlock_shared();

               if (exit)
                   break;
           }

           std::atomic_fetch_add(&readProcessedCnt, localProcessCnt);

       }));

       writeThreads.push_back(std::thread([&data, &exit, &shared_mtx_lock, &writeProcessedCnt]() {

           int localProcessCnt = 0;

           while (true)
           {
               shared_mtx_lock.lock();

               data.push_back(rand() % 100);
               ++localProcessCnt;

               shared_mtx_lock.unlock();

               if (exit)
                   break;
           }

           std::atomic_fetch_add(&writeProcessedCnt, localProcessCnt);

       }));
   }

   std::this_thread::sleep_for(std::chrono::milliseconds(MAIN_WAIT_MILLISECONDS));
   exit = true;

   for (auto &r : readThreads)
       r.join();

   for (auto &w : writeThreads)
       w.join();

   std::cout << "STLSharedTimedMutex READ :      " << readProcessedCnt << std::endl;
   std::cout << "STLSharedTimedMutex WRITE :     " << writeProcessedCnt << std::endl;
   std::cout << "TOTAL READ&WRITE :              " << readProcessedCnt + writeProcessedCnt << std::endl << std::endl;
}

int main()
{
   useSTLSharedTimedMutex();

   return EXIT_SUCCESS;
}
EOF

(
   cd "${OSXCROSS_TEST_DIR}/" \
   && "${OSXCROSS_TEST_TOOLCHAIN_CXX}" \
      -O6 -Wall -g \
      -static-libgcc \
      -std=c++14 \
      ./"${CXPROG05_SOURCE}" \
      -o ./"${CXPROG05_BINARY}" \
   && file ./"${CXPROG05_BINARY}" \
   && xcrun otool -arch all -hvL ./"${CXPROG05_BINARY}" \
)

################################################################################
# CXPROG06: C++17 Program Using ::std::string_view:
################################################################################

echo
echo "======================================================================"
echo " CXPROG06: C++17 Program Using ::std::string_view:"
echo "======================================================================"

CXPROG06_PREFIX="CXPROG06_c++17_std_string_view"
CXPROG06_SOURCE="${CXPROG06_PREFIX}.cpp"
CXPROG06_BINARY="${CXPROG06_PREFIX}.bin.${OSXCROSS_TEST_ARCH}"
cat <<EOF >"${OSXCROSS_TEST_DIR}/${CXPROG06_SOURCE}"
// CXPROG06: C++17 Program Using ::std::string_view:
#include <cstdlib>
#include <iostream>
#include <string_view>

int main()
{
   const ::std::string_view str_1{ "Hello World!" };
   const ::std::string_view str_2{ str_1 };
   const ::std::string_view str_3{ str_2 };
   std::cout << str_1 << '\n' << str_2 << '\n' << str_3 << '\n';
   return EXIT_SUCCESS;
}
EOF

(
   cd "${OSXCROSS_TEST_DIR}/" \
   && "${OSXCROSS_TEST_TOOLCHAIN_CXX}" \
      -O6 -Wall -g \
      -static-libgcc \
      -std=c++17 \
      ./"${CXPROG06_SOURCE}" \
      -o ./"${CXPROG06_BINARY}" \
   && file ./"${CXPROG06_BINARY}" \
   && xcrun otool -arch all -hvL ./"${CXPROG06_BINARY}" \
)

################################################################################
# CXPROG07: C++17 Program Using ::std::shared_mutex:
################################################################################

echo
echo "======================================================================"
echo " CXPROG07: C++17 Program Using ::std::shared_mutex:"
echo "======================================================================"

CXPROG07_PREFIX="CXPROG07_c++17_std_string_view"
CXPROG07_SOURCE="${CXPROG07_PREFIX}.cpp"
CXPROG07_BINARY="${CXPROG07_PREFIX}.bin.${OSXCROSS_TEST_ARCH}"
cat <<EOF >"${OSXCROSS_TEST_DIR}/${CXPROG07_SOURCE}"
// CXPROG07: C++17 Program Using ::std::shared_mutex:
#include <cstdlib>
#include <iostream>
#include <thread>
#include <shared_mutex>

static int value = 0;
static std::shared_mutex mutex;

// Reads the value and sets v to that value
void readValue(int& v)
{
   mutex.lock_shared();
   // Simulate some latency
   std::this_thread::sleep_for(std::chrono::seconds(1));
   v = value;
   mutex.unlock_shared();
}

// Sets value to v
void setValue(int v)
{
   mutex.lock();
   // Simulate some latency
   std::this_thread::sleep_for(std::chrono::seconds(1));
   value = v;
   mutex.unlock();
}

int main()
{
   int read1;
   int read2;
   int read3;
   std::thread t1(readValue, std::ref(read1));
   std::thread t2(readValue, std::ref(read2));
   std::thread t3(readValue, std::ref(read3));
   std::thread t4(setValue, 1);

   t1.join();
   t2.join();
   t3.join();
   t4.join();

   std::cout << read1 << "\n";
   std::cout << read2 << "\n";
   std::cout << read3 << "\n";
   std::cout << value << "\n";

   return EXIT_SUCCESS;
}
EOF

(
   cd "${OSXCROSS_TEST_DIR}/" \
   && "${OSXCROSS_TEST_TOOLCHAIN_CXX}" \
      -O6 -Wall -g \
      -static-libgcc \
      -std=c++17 \
      ./"${CXPROG07_SOURCE}" \
      -o ./"${CXPROG07_BINARY}" \
   && file ./"${CXPROG07_BINARY}" \
   && xcrun otool -arch all -hvL ./"${CXPROG07_BINARY}" \
)
