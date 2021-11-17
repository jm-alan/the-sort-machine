#define FMT_HEADER_ONLY

#include <vector>
#include <future>
#include <mutex>
#include <windows.h>
#include <random>
#include <chrono>
#include <string>
#include <fmt/format.h>
#include <console.h>
#include <linkedlist.h>

#define now() std::chrono::system_clock::now()

#define syncWatch(timer) timer = now()

#define timerInMS(timer) std::chrono::duration_cast<std::chrono::milliseconds>(now() - timer).count()

void trash(std::vector<List *> &ptrV)
{
  std::vector<List *> trashV;
  ptrV.swap(trashV);
};

void alloc(
    List *ptrL,
    const unsigned long long ceil,
    std::mutex *lockAlloc)
{
  std::random_device rd;
  std::default_random_engine generator{rd()};
  std::uniform_int_distribution<unsigned long long> distribution{0, 0xFFFFFFFFFFFFFFFF};
  for (int i{}; i < ceil; i++)
    ptrL->pushNode(new Node(distribution(generator)));
  // vect->push_back(rand());
  lockAlloc->unlock();
};

void sort(
    List *ptrL,
    std::mutex *lockAlloc,
    std::mutex *lockSort)
{
  std::lock_guard<std::mutex> guardL{*lockAlloc};
  ptrL->sort();
  lockSort->unlock();
}

void merge(
    List *ptrL,
    List *ptrR,
    List *ptrAcc,
    std::mutex *lockL,
    std::mutex *lockR,
    std::mutex *lockAcc)
{
  List::threadmerge(ptrL, ptrR, ptrAcc, lockL, lockR);
  lockAcc->unlock();
}

int main()
{
  const unsigned int queriedCPUs{std::thread::hardware_concurrency()};
  console::inl("Queried available logical processors: ");
  console::log(queriedCPUs);
  console::log("Is this correct? [y/n]");
  char yesno{};
  std::cin >> yesno;
  unsigned long long threads{queriedCPUs};
  if (yesno != 'y' && yesno != 'Y')
  {
    console::log("Please input the correct number of threads available on your machine: ");
    std::cin >> threads;
  }

  unsigned long long countSteps{};
  unsigned long long threadsToMerge{threads};
  while (threadsToMerge > 1)
  {
    countSteps++;
    threadsToMerge /= 2;
    if (threadsToMerge > 2 && threadsToMerge % 2)
      countSteps++;
  }

  yesno = 0;
  MEMORYSTATUSEX mstax{};
  mstax.dwLength = sizeof(mstax);
  GlobalMemoryStatusEx(&mstax);
  const unsigned long long physMem{mstax.ullAvailPhys / (1024 * 1024)};
  const unsigned long long totMem{mstax.ullTotalPhys / (1024 * 1024)};
  console::inl("Queried available memory in MB: ");
  console::inl(physMem);
  console::inl(" of ");
  console::log(totMem);
  console::log("Is this roughly correct? [y/n]");
  std::cin >> yesno;
  unsigned long long mem{physMem};
  if (yesno != 'y' && yesno != 'Y')
  {
    console::log("Please input the current amount of AVAILABLE (total - occupied) memory in your machine, in MB:");
    std::cin >> mem;
  }

  // const unsigned long long totalSort{(((mem / 32) * 1000000) / threads) * threads};
  // const unsigned long long totalSort{(50000000 / threads) * threads};
  unsigned long long totalSort{};
  std::cin >> totalSort;

  console::inl("Max sortable long ints: ");
  console::log(totalSort);

  std::vector<std::mutex *> locksAlloc, locksSorts{}, locksSieves{};
  std::vector<std::future<void>> allocators{}, sorters{}, mergers{};
  std::vector<List *> sieves{}, lists{};

  // Start an absolute and per-area timer
  const std::chrono::system_clock::time_point globalTimer{now()};
  std::chrono::system_clock::time_point runningTimer{globalTimer};

  for (int i{}; i < threads; i++)
  {
    lists.push_back(new List());
    locksAlloc.push_back(new std::mutex);
    locksAlloc[i]->lock();
    allocators.push_back(std::async(std::launch::async, alloc, lists[i], totalSort / threads, locksAlloc[i]));
  }

  for (int i{}; i < threads; i++)
  {
    locksSorts.push_back(new std::mutex);
    locksSorts[i]->lock();
    sorters.push_back(std::async(std::launch::async, sort, lists[i], locksAlloc[i], locksSorts[i]));
  }

  for (int i{}; i < threads; i += 2)
  {
    sieves.push_back(new List());
    locksSieves.push_back(new std::mutex);
    locksSieves[locksSieves.size() - 1]->lock();
    mergers.push_back(std::async(
        std::launch::async,
        merge,
        lists[i],
        lists[i + 1],
        sieves[sieves.size() - 1],
        locksSorts[i],
        locksSorts[i + 1],
        locksSieves[locksSieves.size() - 1]));
  }

  int sPos{};
  int timesMerged{};

  while (timesMerged < countSteps)
  {
    timesMerged++;
    const size_t sieveSizeNow = sieves.size();

    for (; sPos + 1 < sieveSizeNow; sPos += 2)
    {
      sieves.push_back(new List());
      locksSieves.push_back(new std::mutex);
      locksSieves[locksSieves.size() - 1]->lock();
      mergers.push_back(std::async(
          std::launch::async,
          merge,
          sieves[sPos],
          sieves[sPos + 1],
          sieves[sieves.size() - 1],
          locksSieves[sPos],
          locksSieves[sPos + 1],
          locksSieves[locksSieves.size() - 1]));
    }
  }

  for (int i = 0; i < mergers.size(); i++)
    mergers[i].wait();

  console::inl("Merge completed in ");
  console::inl(timerInMS(runningTimer));
  console::log(" milliseconds");

  const auto finalTimer = timerInMS(globalTimer);

  console::log("Total operation for allocating, sorting, and merging completed in");
  console::inl(finalTimer);
  console::inl(" milliseconds, for a total average rate of ");
  console::inl(totalSort / finalTimer);
  console::log(" ints/ms");

  const List *finL{sieves[sieves.size() - 1]};
  trash(sieves);
  // Uncomment the lines below if you want to print the sorted vector when it's finished
  // Node *current = finL->head;
  // if (current != nullptr)
  //   console::log(current->val);
  // while (current != nullptr)
  // {
  //   console::log(current->val);
  //   current = current->next;
  // }
  Node *dest{finL->head};
  while (dest != nullptr)
  {
    Node *unheap{dest};
    dest = dest->next;
    delete unheap;
  }
  delete finL->head;
  delete finL->tail;
  system("pause");
}
