/* Filesystem - sample usage.
 *
 * The testing of the fs driver requires a backend (simulating the underlying disk block device).
 * Next, tests of your fs implemetnation are needed. To help you with the implementation,
 * a sample backend is implemented in this file. It provides a quick-and-dirty
 * implementation of the underlying disk (simulated in a file) and a few CFileSystem calls.
 *
 * The implementation in the real testing environment is different. The sample below is a
 * minimalistic disk backend which matches the required interface.
 *
 * You will have to add some FS testing. There are some CFileSystem methods called from within
 * main(), however, the tests are incomplete. Once again, this is only a starting point.
 */


//constexpr int DISK_SECTORS = 8192;
//constexpr int DISK_SECTORS = 8192 * 128;
constexpr int DISK_SECTORS = 1024 * 1024 * 2;
static FILE *g_Fp = nullptr;

//-------------------------------------------------------------------------------------------------
/** Sample sector reading function. The function will be called by your fs driver implementation.
 * Notice, the function is not called directly. Instead, the function will be invoked indirectly
 * through function pointer in the TBlkDev structure.
 */
static size_t diskRead(size_t sectorNr, void *data, size_t sectorCnt) {
  if (g_Fp == nullptr || sectorNr + sectorCnt > DISK_SECTORS) return 0;
  fseek(g_Fp, sectorNr * SECTOR_SIZE, SEEK_SET);
  return fread(data, SECTOR_SIZE, sectorCnt, g_Fp);
}

//-------------------------------------------------------------------------------------------------
/** Sample sector writing function. Similar to diskRead
 */
static size_t diskWrite(size_t sectorNr, const void *data, size_t sectorCnt) {
  if (g_Fp == nullptr || sectorNr + sectorCnt > DISK_SECTORS)
    return 0;
  fseek(g_Fp, sectorNr * SECTOR_SIZE, SEEK_SET);
  return fwrite(data, SECTOR_SIZE, sectorCnt, g_Fp);
}

//-------------------------------------------------------------------------------------------------
/** A function which creates the file needed for the sector reading/writing functions above.
 * This function is only needed for the particular implementation above. It could be understand as
 * "buying a new disk".
 */
static TBlkDev createDisk() {
  char buffer[SECTOR_SIZE];

  memset(buffer, 0, sizeof (buffer));
  g_Fp = fopen("/tmp/disk_content", "w+b");
  if (!g_Fp) throw "Error creating backed block device";

  for (int i = 0; i < DISK_SECTORS; i++)
    if (fwrite(buffer, sizeof (buffer), 1, g_Fp) != 1) throw "Error creating backed block device";

  TBlkDev res;
  res.m_Sectors = DISK_SECTORS;
  res.m_Read = diskRead;
  res.m_Write = diskWrite;
  return res;
}

//-------------------------------------------------------------------------------------------------
/** A function which opens the files needed for the sector reading/writing functions above.
 * This function is only needed for the particular implementation above. It could be understand as
 * "turning the computer on".
 */
static TBlkDev openDisk() {
  g_Fp = fopen("/tmp/disk_content", "r+b");
  if (!g_Fp) throw "Error opening backend block device";
  fseek(g_Fp, 0, SEEK_END);
  if (ftell(g_Fp) != DISK_SECTORS * SECTOR_SIZE) {
    fclose(g_Fp);
    g_Fp = nullptr;
    throw "Error opening backend block device";
  }

  TBlkDev res;
  res.m_Sectors = DISK_SECTORS;
  res.m_Read = diskRead;
  res.m_Write = diskWrite;
  return res;
}

//-------------------------------------------------------------------------------------------------
/** A function which releases resources allocated by openDisk/createDisk
 */
static void doneDisk() {
  if (g_Fp) {
    fclose(g_Fp);
    g_Fp = nullptr;
  }
}

//-------------------------------------------------------------------------------------------------
static void testMkFs() {
  /* Create the disk backend and format it using youe FsCreate call
   */

  assert(CFileSystem::createFs ( createDisk () ));
  doneDisk();
}

//-------------------------------------------------------------------------------------------------
__attribute__((no_sanitize("address")))
static void testWrite() {
  char buffer[100];
  TFile info;
  /**
   * mount the peviously created fs
   */
  CFileSystem *fs = CFileSystem::mount(openDisk());
  assert(fs);

  /* your fs shall be ready. Try to create a file named "hello" inside your filesystem:
   */

  int fd = fs->openFile("hello", true);

  for (int i = 0; i < 100; i++) {
    buffer[i] = i;
  }

  assert(fs -> writeFile ( fd, buffer, 100 ) == 100);
  assert(fs -> closeFile ( fd ));

  /* test other fs operations here */


  /* umount the filesystem
   */
  assert(fs -> umount ());
  delete fs;

  /* ... and the underlying disk.
   */
  doneDisk();


  /* The FS as well as the underlying disk simulation is stopped. It corresponds i.e. to the
   * restart of a real computer.
   *
   * after the restart, we will not create the disk,  nor create FS (we do not
   * want to destroy the content). Instead, we will only open/mount the devices:
   */


  fs = fs->mount(openDisk());
  assert(fs);
  /* some I/O, tests, list the files
   */

  for (bool found = fs->findFirst(info); found; found = fs->findNext(info))
    printf("%-30s %6zd\n", info.m_FileName, info.m_FileSize);

  assert(fs -> umount ());
  delete fs;
  doneDisk();
  printf("TEST FINISHED\n");
}

#include <vector>
#include <iostream>
#include <iomanip>

__attribute__((no_sanitize("address")))
static void testSmallFilesOnlyWrite() {
  TFile info;
  const size_t fileCount = 20;
  const size_t fileSize = 4096; // 4 KiB

  // Prepare disk and format it
  assert(CFileSystem::createFs(createDisk()));
  doneDisk();

  // First mount
  CFileSystem *fs = CFileSystem::mount(openDisk());
  assert(fs);

  // Write multiple small files (each up to 4KiB)
  for (size_t i = 0; i < fileCount; ++i) {
    char fileName[20];
    snprintf(fileName, sizeof(fileName), "file_%zu", i);

    int fd = fs->openFile(fileName, true);
    assert(fd >= 0);

    std::vector<char> data(fileSize);
    for (size_t j = 0; j < fileSize; ++j)
      data[j] = static_cast<char>((i + j) % 256); // Fill with arbitrary pattern

    assert(fs->writeFile(fd, data.data(), data.size()) == data.size());
    assert(fs->closeFile(fd));
  }

  // Umount and delete instance
  assert(fs->umount());
  delete fs;
  doneDisk();

  // Remount and verify file list
  fs = CFileSystem::mount(openDisk());
  assert(fs);

  size_t listed = 0;
  std::cout << "KAKAYA TO EBATORIYA S FAILAMI" << std::endl;
  /*for (bool found = fs->findFirst(info); found; found = fs->findNext(info)) {
    std::cout << "Found file: " << std::left << std::setw(16)
              << info.m_FileName << " size: " << info.m_FileSize << std::endl;
    listed++;
  }*/

  std::cout << "ALREADY HAVE: " << info.m_FileName << std::endl;

  bool found = fs->findFirst(info);
  std::cout << "Found " << info.m_FileName << std::endl;
  bool found2 = fs->findNext(info);
  std::cout << "Found " << info.m_FileName << std::endl;
  bool found3 = fs->findNext(info);
  std::cout << "Found " << info.m_FileName << std::endl;
  bool found4 = fs->findNext(info);
  std::cout << "Found " << info.m_FileName << std::endl;
  bool found5 = fs->findNext(info);
  std::cout << "Found " << info.m_FileName << std::endl;

  // Just a sanity check
  //assert(listed == fileCount);

  assert(fs->umount());
  delete fs;
  doneDisk();

  std::cout << "testSmallFilesOnlyWrite PASSED" << std::endl;
}

__attribute__((no_sanitize("address")))
static void testEdgeCaseFiles() {
  const size_t fileSizeSmall = 4096;
  TFile info;

  // Prepare disk and format it
  assert(CFileSystem::createFs(createDisk()));
  doneDisk();

  CFileSystem *fs = CFileSystem::mount(openDisk());
  assert(fs);

  std::vector<std::pair<std::string, size_t>> testCases = {
    {"", 100},                                // Empty name
    //{std::string("\0\0\0\0\0", 5), 100},      // All-null characters (non-printable)
    {"/\\:*?\"<>|", 100},                     // Name with unusual/special characters
    {std::string(FILENAME_LEN_MAX, 'A'), 100},// Max length name
    {"zero_size", 0},                         // File with 0 bytes
    {"exact_4kib", 4096},                     // File with exactly 4 KiB
    {"tiny", 1}                               // File with 1 byte
  };

  for (const auto &tc : testCases) {
    const std::string &fileName = tc.first;
    const size_t size = tc.second;

    int fd = fs->openFile(fileName.c_str(), true);
    assert(fd >= 0);

    std::vector<char> buffer(size);
    for (size_t j = 0; j < size; ++j)
      buffer[j] = static_cast<char>((j + fileName.length()) % 256); // Arbitrary pattern

    assert(fs->writeFile(fd, buffer.data(), buffer.size()) == buffer.size());
    assert(fs->closeFile(fd));
  }

  assert(fs->umount());
  delete fs;
  doneDisk();

  // Remount and verify files
  fs = CFileSystem::mount(openDisk());
  assert(fs);

  size_t listed = 0;
  fs->findFirst(info);
  std::cout << "Found " << info.m_FileName << std::endl;
  for (bool found = fs->findFirst(info); found; found = fs->findNext(info)) {
    std::cout << "Found file: \"" << std::string(info.m_FileName, strnlen(info.m_FileName, FILENAME_LEN_MAX))
              << "\" size: " << info.m_FileSize << "\n";
    listed++;
  }

  std::cout << "Edge case test listed " << listed << " files\n";
  assert(listed == testCases.size());

  assert(fs->umount());
  delete fs;
  doneDisk();

  std::cout << "testEdgeCaseFiles PASSED\n";
}

__attribute__((no_sanitize("address")))
static void testTriggerSegfaultWithSmallFiles() {
  const size_t fileCount = 128;
  const size_t fileSize = 1;
  TFile info;

  std::cout << "Starting testTriggerSegfaultWithSmallFiles\n";

  assert(CFileSystem::createFs(createDisk()));
  doneDisk();

  CFileSystem *fs = CFileSystem::mount(openDisk());
  assert(fs);

  for (size_t i = 0; i < fileCount; ++i) {
    char fileName[32];
    snprintf(fileName, sizeof(fileName), "file_%04zu", i);

    int fd = fs->openFile(fileName, true);
    if (fd < 0) {
      std::cout << "Failed to open file: " << fileName << " at index " << i << std::endl;
      break;
    }

    char data = static_cast<char>(i % 256);
    if (fs->writeFile(fd, &data, 1) != 1) {
      std::cout << "Failed to write file: " << fileName << std::endl;
      break;
    }

    fs->closeFile(fd);
  }

  fs->umount();
  delete fs;
  doneDisk();

  std::cout << "testTriggerSegfaultWithSmallFiles FINISHED\n";
}

__attribute__((no_sanitize("address")))
static void testForceSegfaultFileReuse() {
  constexpr size_t fileCount = 64;
  constexpr size_t fileSize = 4096; // 4 KiB
  std::vector<int> fds;
  TFile info;

  std::cout << "Starting testForceSegfaultFileReuse...\n";

  assert(CFileSystem::createFs(createDisk()));
  doneDisk();

  CFileSystem *fs = CFileSystem::mount(openDisk());
  assert(fs);

  // Первый проход: создаём и записываем файлы
  for (size_t i = 0; i < fileCount; ++i) {
    char fileName[32];
    snprintf(fileName, sizeof(fileName), "file_%zu", i);

    int fd = fs->openFile(fileName, true);
    assert(fd >= 0);

    std::vector<char> data(fileSize, static_cast<char>(i % 256));
    assert(fs->writeFile(fd, data.data(), data.size()) == data.size());
    assert(fs->closeFile(fd));
  }

  // Второй проход: открываем те же файлы на запись и обнуляем (truncate)
  for (size_t i = 0; i < fileCount; ++i) {
    char fileName[32];
    snprintf(fileName, sizeof(fileName), "file_%zu", i);

    int fd = fs->openFile(fileName, true);
    assert(fd >= 0);

    // Пишем меньше данных (например, 1 байт)
    char byte = static_cast<char>(i);
    assert(fs->writeFile(fd, &byte, 1) == 1);
    assert(fs->closeFile(fd));
  }

  // Третий проход: снова перезаписываем
  for (size_t i = 0; i < fileCount; ++i) {
    char fileName[32];
    snprintf(fileName, sizeof(fileName), "file_%zu", i);

    int fd = fs->openFile(fileName, true);
    assert(fd >= 0);

    std::vector<char> data(fileSize * 2, static_cast<char>((i * 3) % 256));
    assert(fs->writeFile(fd, data.data(), data.size()) == data.size());
    assert(fs->closeFile(fd));
  }

  assert(fs->umount());
  delete fs;
  doneDisk();

  std::cout << "testForceSegfaultFileReuse FINISHED\n";
}
//-------------------------------------------------------------------------------------------------
static void testBufferOverflow () {
  char writeBuf[4608]; // 9 секторов * 512
  for (int i = 0; i < 4608; ++i)
    writeBuf[i] = (char)(i % 256);

  // 1. Форматирование и запись
  assert(CFileSystem::createFs(createDisk()));
  CFileSystem *fs = CFileSystem::mount(openDisk());
  assert(fs);

  int fd = fs->openFile("bigdata", true);
  assert(fd != -1);
  assert(fs->writeFile(fd, writeBuf, sizeof(writeBuf)) == sizeof(writeBuf));
  assert(fs->closeFile(fd));
  assert(fs->umount());
  delete fs;
  doneDisk();

  // 2. Перемонтируем и читаем
  fs = CFileSystem::mount(openDisk());
  assert(fs);

  char readBuf[4608]; // тот же размер
  fd = fs->openFile("bigdata", false);
  assert(fd != -1);
  size_t readBytes = fs->readFile(fd, readBuf, sizeof(readBuf));
  assert(readBytes == sizeof(readBuf));

  // Проверка содержимого
  for (int i = 0; i < 4608; ++i)
    assert(readBuf[i] == (char)(i % 256));

  assert(fs->closeFile(fd));
  assert(fs->umount());
  delete fs;
  doneDisk();
}
static void testWriteReadConsistency ()
{
  // Делаем буферы на 4608 байт (9 секторов)
  char writeBuf[4608], readBuf[4608];

  // Заполняем шаблоном
  for (int i = 0; i < 4608; ++i)
    writeBuf[i] = (char)(i % 256);

  // Создаём диск и форматируем
  assert(CFileSystem::createFs(createDisk()));
  doneDisk();  ///// <- NOT WORKING WITHOUT THIS SHIT

  // Монтируем файловую систему
  CFileSystem *fs = CFileSystem::mount(openDisk());
  assert(fs);

  // Открываем файл на запись
  int fd = fs->openFile("testfile", true);
  assert(fd != -1);

  // Пишем 4608 байт
  assert(fs->writeFile(fd, writeBuf, sizeof(writeBuf)) == sizeof(writeBuf));

  // Закрываем
  assert(fs->closeFile(fd));

  // Открываем снова на чтение
  fd = fs->openFile("testfile", false);
  assert(fd != -1);

  // Читаем в readBuf
  size_t read = fs->readFile(fd, readBuf, sizeof(readBuf));
  assert(read == sizeof(readBuf));

  // Сравниваем побайтово
  for (int i = 0; i < 4608; ++i)
    assert(readBuf[i] == writeBuf[i]);

  // Закрываем
  assert(fs->closeFile(fd));

  // Всё ок
  delete fs;
  doneDisk();
}
static void testWriteReadConsistency2()
{
  char writeBuf[100];
  char readBuf[100];

  for (int i = 0; i < 100; ++i)
    writeBuf[i] = i;

  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  doneDisk();

  CFileSystem *fs = CFileSystem::mount(openDisk());
  assert(fs);

  int fd = fs->openFile("file1", true);
  assert(fd != -1);
  assert(fs->writeFile(fd, writeBuf, 100) == 100);
  assert(fs->closeFile(fd));
  assert(fs->umount());
  delete fs;
  doneDisk();

  fs = CFileSystem::mount(openDisk());
  assert(fs);
  fd = fs->openFile("file1", false);
  assert(fd != -1);
  assert(fs->readFile(fd, readBuf, 100) == 100);
  assert(fs->closeFile(fd));
  assert(memcmp(writeBuf, readBuf, 100) == 0);
  assert(fs->umount());
  delete fs;
  doneDisk();
}
static void testMultipleFilesReadBack()
{
  char buf1[50], buf2[75], buf3[100];
  for (int i = 0; i < 50; ++i) buf1[i] = 'A' + i;
  for (int i = 0; i < 75; ++i) buf2[i] = 'a' + (i % 26);
  for (int i = 0; i < 100; ++i) buf3[i] = i;

  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  doneDisk();

  CFileSystem *fs = CFileSystem::mount(openDisk());
  assert(fs);

  int f1 = fs->openFile("f1", true);
  int f2 = fs->openFile("f2", true);
  int f3 = fs->openFile("f3", true);

  assert(f1 != -1 && f2 != -1 && f3 != -1);
  assert(fs->writeFile(f1, buf1, 50) == 50);
  assert(fs->writeFile(f2, buf2, 75) == 75);
  assert(fs->writeFile(f3, buf3, 100) == 100);
  fs->closeFile(f1);
  fs->closeFile(f2);
  fs->closeFile(f3);
  assert(fs->umount());
  delete fs;
  doneDisk();

  fs = CFileSystem::mount(openDisk());
  assert(fs);
  char out1[50], out2[75], out3[100];
  f1 = fs->openFile("f1", false);
  f2 = fs->openFile("f2", false);
  f3 = fs->openFile("f3", false);
  assert(fs->readFile(f1, out1, 50) == 50);
  assert(fs->readFile(f2, out2, 75) == 75);
  assert(fs->readFile(f3, out3, 100) == 100);
  assert(memcmp(buf1, out1, 50) == 0);
  assert(memcmp(buf2, out2, 75) == 0);
  assert(memcmp(buf3, out3, 100) == 0);
  fs->closeFile(f1);
  fs->closeFile(f2);
  fs->closeFile(f3);
  assert(fs->umount());
  delete fs;
  doneDisk();
}
static void testSmallFilesContentCheck()
{
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));

  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  const int fileCount = 3;
  const char *names[fileCount] = { "a.txt", "b.txt", "c.txt" };
  const int sizes[fileCount] = { 100, 200, 512 };
  char data[fileCount][4096];
  char readBack[4096];

  for (int i = 0; i < fileCount; ++i)
  {
    for (int j = 0; j < sizes[i]; ++j)
      data[i][j] = (char)((j + i) % 256);  // уникальный паттерн

    int fd = fs->openFile(names[i], true);
    assert(fd != -1);
    assert(fs->writeFile(fd, data[i], sizes[i]) == (size_t)sizes[i]);
    assert(fs->closeFile(fd));
  }

  for (int i = 0; i < fileCount; ++i)
  {
    int fd = fs->openFile(names[i], false);
    assert(fd != -1);
    size_t read = fs->readFile(fd, readBack, sizes[i]);
    assert(read == (size_t)sizes[i]);
    assert(memcmp(readBack, data[i], sizes[i]) == 0);
    assert(fs->closeFile(fd));
  }

  assert(fs->umount());
  delete fs;
  doneDisk();
}
static void testSmallFilesChunkedRead()
{
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));

  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  const int fileCount = 10;
  char name[32];
  char data[fileCount][4096];
  char readBuf[4096];

  for (int i = 0; i < fileCount; ++i)
  {
    snprintf(name, sizeof(name), "file_%d.txt", i);
    int size = 100 + (i * 123) % 4000;

    for (int j = 0; j < size; ++j)
      data[i][j] = (char)((i * j + 7) % 256);

    int fd = fs->openFile(name, true);
    assert(fd != -1);
    assert(fs->writeFile(fd, data[i], size) == (size_t)size);
    assert(fs->closeFile(fd));
  }

  for (int i = 0; i < fileCount; ++i)
  {
    snprintf(name, sizeof(name), "file_%d.txt", i);
    int size = 100 + (i * 123) % 4000;

    int fd = fs->openFile(name, false);
    assert(fd != -1);

    size_t totalRead = 0;
    while (totalRead < (size_t)size)
    {
      size_t toRead = std::min<size_t>(137, size - totalRead);
      size_t actual = fs->readFile(fd, readBuf + totalRead, toRead);
      //std::cout << "Expected " << toRead << ", got " << actual << ", total so far: " << totalRead << ", file size: " << size << "\n";

      assert(actual == toRead);
      totalRead += actual;
    }

    assert(memcmp(readBuf, data[i], size) == 0);
    assert(fs->closeFile(fd));
  }

  assert(fs->umount());
  delete fs;
  doneDisk();
}
void testWriteRead_SingleSector()
{
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  char writeBuf[SECTOR_SIZE - 10];
  for (size_t i = 0; i < sizeof(writeBuf); ++i)
    writeBuf[i] = i % 256;

  int fd = fs->openFile("file1", true);
  assert(fd != -1);
  assert(fs->writeFile(fd, writeBuf, sizeof(writeBuf)) == sizeof(writeBuf));
  assert(fs->closeFile(fd));

  fd = fs->openFile("file1", false);
  assert(fd != -1);
  char readBuf[SECTOR_SIZE - 10] = {};
  assert(fs->readFile(fd, readBuf, sizeof(readBuf)) == sizeof(readBuf));
  assert(fs->closeFile(fd));

  for (size_t i = 0; i < sizeof(writeBuf); ++i)
    assert(writeBuf[i] == readBuf[i]);

  assert(fs->umount());
  delete fs;
  doneDisk();
}
void testWriteRead_TwoSectors()
{
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  const size_t size = SECTOR_SIZE * 2;
  char writeBuf[size];
  for (size_t i = 0; i < size; ++i)
    writeBuf[i] = (i * 3) % 256;

  int fd = fs->openFile("file2", true);
  assert(fd != -1);
  assert(fs->writeFile(fd, writeBuf, size) == size);
  assert(fs->closeFile(fd));

  fd = fs->openFile("file2", false);
  assert(fd != -1);
  char readBuf[size] = {};
  assert(fs->readFile(fd, readBuf, size) == size);
  assert(fs->closeFile(fd));

  for (size_t i = 0; i < size; ++i)
    assert(writeBuf[i] == readBuf[i]);

  assert(fs->umount());
  delete fs;
  doneDisk();
}
void testWriteRead_4096Chunked()
{
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  const size_t fileSize = 409600;
  char writeBuf[fileSize];
  for (size_t i = 0; i < fileSize; ++i)
    writeBuf[i] = (i * 17 + 3) % 256;

  int fd = fs->openFile("bigchunk", true);
  assert(fd != -1);
  assert(fs->writeFile(fd, writeBuf, fileSize) == fileSize);
  assert(fs->closeFile(fd));

  fd = fs->openFile("bigchunk", false);
  assert(fd != -1);

  char readBuf[fileSize] = {};
  size_t totalRead = 0;
  while (totalRead < fileSize)
  {
    size_t chunk = std::min<size_t>(137, fileSize - totalRead);
    size_t read = fs->readFile(fd, readBuf + totalRead, chunk);
    assert(read == chunk);
    totalRead += read;
  }
  assert(fs->closeFile(fd));

  for (size_t i = 0; i < fileSize; ++i)
    assert(readBuf[i] == writeBuf[i]);

  assert(fs->umount());
  delete fs;
  doneDisk();
}
void testMultiFile4096()
{
  const int numFiles = 5;
  const size_t fileSize = 409600;
  const char *names[numFiles] = { "f1", "f2", "f3", "f4", "f5" };
  char writeBuf[numFiles][fileSize];
  char readBuf[fileSize];

  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  // Init data
  for (int i = 0; i < numFiles; ++i)
    for (size_t j = 0; j < fileSize; ++j)
      writeBuf[i][j] = (j * (i + 3) + i * 17) % 256;

  // Write all files
  for (int i = 0; i < numFiles; ++i)
  {
    int fd = fs->openFile(names[i], true);
    assert(fd != -1);
    assert(fs->writeFile(fd, writeBuf[i], fileSize) == fileSize);
    assert(fs->closeFile(fd));
  }

  // Read and verify all files
  for (int i = 0; i < numFiles; ++i)
  {
    int fd = fs->openFile(names[i], false);
    assert(fd != -1);

    size_t totalRead = 0;
    while (totalRead < fileSize)
    {
      size_t chunk = (i % 2 == 0) ? 211 : 137;
      chunk = std::min(chunk, fileSize - totalRead);
      size_t actual = fs->readFile(fd, readBuf + totalRead, chunk);
      assert(actual == chunk);
      totalRead += actual;
    }
    assert(fs->closeFile(fd));

    for (size_t j = 0; j < fileSize; ++j)
      assert(readBuf[j] == writeBuf[i][j]);
  }

  assert(fs->umount());
  delete fs;
  doneDisk();
}
void testManyFilesVariousSizes()
{
  const int numFiles = 20;
  const size_t maxSize = 4096000;
  std::string names[numFiles];
  std::vector<std::vector<char>> data(numFiles);

  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  // Генерация имен и данных
  for (int i = 0; i < numFiles; ++i)
  {
    names[i] = "file_" + std::to_string(i);
    size_t sz = 100 + (i * 199) % maxSize; // от 100 до 4096
    data[i].resize(sz);

    for (size_t j = 0; j < sz; ++j)
      data[i][j] = static_cast<char>((j * (i + 7)) % 256);
  }

  // Запись всех файлов
  for (int i = 0; i < numFiles; ++i)
  {
    int fd = fs->openFile(names[i].c_str(), true);
    assert(fd != -1);
    assert(fs->writeFile(fd, data[i].data(), data[i].size()) == data[i].size());
    assert(fs->closeFile(fd));
  }

  // Проверка чтения по кускам
  for (int i = 0; i < numFiles; ++i)
  {
    std::vector<char> readBuf(data[i].size(), 0);
    int fd = fs->openFile(names[i].c_str(), false);
    assert(fd != -1);

    size_t totalRead = 0;
    while (totalRead < data[i].size())
    {
      size_t chunk = (i % 3 == 0) ? 137 : ((i % 3 == 1) ? 251 : 503);
      chunk = std::min(chunk, data[i].size() - totalRead);
      size_t actual = fs->readFile(fd, readBuf.data() + totalRead, chunk);
      assert(actual == chunk);
      totalRead += actual;
    }
    assert(fs->closeFile(fd));

    for (size_t j = 0; j < data[i].size(); ++j)
      assert(readBuf[j] == data[i][j]);
  }

  assert(fs->umount());
  delete fs;
  doneDisk();
}

void testHundredsOfFilesUpTo4KiB()
{
  const int fileCount = 128;
  const size_t maxFileSize = 1024 * 1024 * 1024 - 1;

  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  std::vector<std::string> names(fileCount);
  std::vector<std::vector<char>> contents(fileCount);

  // Сначала создаем и записываем
  for (int i = 0; i < fileCount; ++i)
  {
    names[i] = "file_" + std::to_string(i);
    size_t sz = 512 + (i * 37) % (maxFileSize - 512); // от 512 до 4096
    contents[i].resize(sz);

    for (size_t j = 0; j < sz; ++j)
      contents[i][j] = static_cast<char>((i * j + 13) % 256);

    int fd = fs->openFile(names[i].c_str(), true);
    assert(fd != -1);
    assert(fs->writeFile(fd, contents[i].data(), sz) == sz);
    assert(fs->closeFile(fd));
  }

  // Теперь читаем и проверяем
  for (int i = 0; i < fileCount; ++i)
  {
    int fd = fs->openFile(names[i].c_str(), false);
    assert(fd != -1);

    std::vector<char> readBuf(contents[i].size(), 0);
    size_t total = 0;

    while (total < contents[i].size())
    {
      size_t chunk = 500 + (i * 71) % 1024;
      chunk = std::min(chunk, contents[i].size() - total);
      size_t actual = fs->readFile(fd, readBuf.data() + total, chunk);
      assert(actual == chunk);
      total += actual;
    }

    assert(fs->closeFile(fd));

    for (size_t j = 0; j < contents[i].size(); ++j)
      assert(readBuf[j] == contents[i][j]);
  }

  assert(fs->umount());
  delete fs;
  doneDisk();
}

void testDeleting() {
  const int fileCount = 128;
  const size_t maxFileSize = 4096000;

  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  std::vector<std::string> names(fileCount);
  std::vector<std::vector<char>> contents(fileCount);

  // Сначала создаем и записываем
  for (int i = 0; i < fileCount; ++i)
  {
    names[i] = "file_" + std::to_string(i);
    size_t sz = 512 + (i * 37) % (maxFileSize - 512); // от 512 до 4096
    contents[i].resize(sz);

    for (size_t j = 0; j < sz; ++j)
      contents[i][j] = static_cast<char>((i * j + 13) % 256);

    int fd = fs->openFile(names[i].c_str(), true);
    assert(fd != -1);
    assert(fs->writeFile(fd, contents[i].data(), sz) == sz);
    assert(fs->closeFile(fd));
  }

  for (int i = 0; i < fileCount; ++i)
  {
    int fd = fs->openFile(names[i].c_str(), false);
    assert(fd != -1);

    std::vector<char> readBuf(contents[i].size(), 0);
    size_t total = 0;

    while (total < contents[i].size())
    {
      size_t chunk = 500 + (i * 71) % 1024;
      chunk = std::min(chunk, contents[i].size() - total);
      size_t actual = fs->readFile(fd, readBuf.data() + total, chunk);
      assert(actual == chunk);
      total += actual;
    }

    assert(fs->closeFile(fd));

    for (size_t j = 0; j < contents[i].size(); ++j)
      assert(readBuf[j] == contents[i][j]);
  }

  // Удаляем по одному файлу и проверяем оставшиеся
  for (int toDelete = 0; toDelete < fileCount; ++toDelete) {
    assert(fs->deleteFile(names[toDelete].c_str())); // Удаляем текущий

    // Проверяем оставшиеся
    for (int i = toDelete + 1; i < fileCount; ++i) {
      int fd = fs->openFile(names[i].c_str(), false);
      std::cout << fd << std::endl;
      assert(fd != -1); // Файл должен быть

      std::vector<char> readBuf(contents[i].size(), 0);
      size_t total = 0;

      while (total < contents[i].size()) {
        size_t chunk = 500 + (i * 71) % 1024;
        chunk = std::min(chunk, contents[i].size() - total);
        const size_t actual = fs->readFile(fd, readBuf.data() + total, chunk);
        assert(actual == chunk);
        total += actual;
      }

      assert(fs->closeFile(fd));

      for (size_t j = 0; j < contents[i].size(); ++j)
        assert(readBuf[j] == contents[i][j]); // Данные не повреждены
    }
  }

  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("LAST TEST FINISHED\n");
}

void microTest() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  int fd = fs->openFile("first", true);
  assert(fd != -1);

  uint8_t buffer[512] = {};
  memcpy(buffer, "data_of_the_first_file", 22);
  assert(fs->writeFile(fd, buffer, 512) == 512);
  assert(fs->closeFile(fd));

  fd = fs->openFile("second", true);
  assert(fd != -1);

  uint8_t buffer2[512] = {};
  memcpy(buffer2, "data_of_the_second_file", 23);
  assert(fs->writeFile(fd, buffer2, 512) == 512);
  assert(fs->closeFile(fd));

  assert(fs->deleteFile("first"));

  uint8_t buffer3[512] = {};
  fd = fs->openFile("second", false);
  assert(fd != -1);
  const size_t actual = fs->readFile(fd, buffer3, 512);
  std::cout << "Read bytes: " << actual << "\n";
  assert(actual == 512);
  assert(fs->closeFile(fd));

  // 👇 Сравнение содержимого
  assert(memcmp(buffer3, buffer2, 512) == 0);

  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("microTest finished\n");
}

/*
void testArbitraryFilesWithDelete() {
  const int fileCount = 64;
  const size_t maxSize = 16384; // до 16 KiB

  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  std::vector<std::string> names(fileCount);
  std::vector<std::vector<char>> contents(fileCount);

  // Генерация и запись файлов
  for (int i = 0; i < fileCount; ++i) {
    names[i] = "file_" + std::to_string(i);
    size_t size = 200 + (i * 791) % maxSize; // от 200 до ~16KiB
    contents[i].resize(size);

    for (size_t j = 0; j < size; ++j)
      contents[i][j] = static_cast<char>((i * j + 73) % 256);

    int fd = fs->openFile(names[i].c_str(), true); // truncate
    assert(fd != -1);
    assert(fs->writeFile(fd, contents[i].data(), size) == size);
    assert(fs->closeFile(fd));
  }

  // Чтение и проверка
  for (int i = 0; i < fileCount; ++i) {
    int fd = fs->openFile(names[i].c_str(), false);
    assert(fd != -1);

    std::vector<char> readBuf(contents[i].size());
    size_t read = fs->readFile(fd, readBuf.data(), readBuf.size());
    assert(read == contents[i].size());
    assert(fs->closeFile(fd));
    assert(memcmp(readBuf.data(), contents[i].data(), readBuf.size()) == 0);
  }

  // Дозапись в некоторые файлы
  for (int i = 0; i < fileCount; i += 3) { // каждый 3-й файл
    std::vector<char> extra(5000); // +5000 байт
    for (size_t j = 0; j < extra.size(); ++j)
      extra[j] = static_cast<char>((i * j + 91) % 256);

    int fd = fs->openFile(names[i].c_str(), true); // append
    assert(fd != -1);
    assert(fs->writeFile(fd, extra.data(), extra.size()) == extra.size());
    assert(fs->closeFile(fd));

    // Обновить ожидание содержимого
    contents[i].insert(contents[i].end(), extra.begin(), extra.end());
  }

  // Проверка всех файлов после дозаписи
  for (int i = 0; i < fileCount; ++i) {
    int fd = fs->openFile(names[i].c_str(), false);
    std::cout << "FILE: " << fd << std::endl;
    assert(fd != -1);

    std::vector<char> readBuf(contents[i].size());
    size_t read = fs->readFile(fd, readBuf.data(), readBuf.size());
    std::cout << "----------------------------------" << std::endl;
    std::cout << "FD: " << fd << std::endl;
    std::cout << "Read bytes: " << read << "\n";
    std::cout << "Reference: " << contents[i].size() << std::endl;
    std::cout << "----------------------------------" << std::endl;
    assert(read == contents[i].size());
    assert(fs->closeFile(fd));
    std::cout << readBuf.data() << std::endl;
    std::cout << contents[i].data() << std::endl;
    assert(memcmp(readBuf.data(), contents[i].data(), readBuf.size()) == 0);
  }

  // Удаление файлов по одному и проверка оставшихся
  for (int toDelete = 0; toDelete < fileCount; ++toDelete) {
    assert(fs->deleteFile(names[toDelete].c_str()));

    for (int i = toDelete + 1; i < fileCount; ++i) {
      int fd = fs->openFile(names[i].c_str(), false);
      assert(fd != -1);

      std::vector<char> readBuf(contents[i].size());
      size_t read = fs->readFile(fd, readBuf.data(), readBuf.size());
      assert(read == contents[i].size());
      assert(fs->closeFile(fd));
      assert(memcmp(readBuf.data(), contents[i].data(), readBuf.size()) == 0);
    }
  }

  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("testArbitraryFilesWithDelete FINISHED\n");
}*/

void testBigFiles() {
  const int fileCount = 64;
  const size_t maxSize = 8 * 1024 * 1024; // до 8 mb

  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  std::vector<std::string> names(fileCount);
  std::vector<std::vector<char>> contents(fileCount);

  // Генерация и запись файлов
  for (int i = 0; i < fileCount; ++i) {
    names[i] = "file_" + std::to_string(i);
    size_t size = 200 + (i * 791) % maxSize;
    contents[i].resize(size);

    for (size_t j = 0; j < size; ++j)
      contents[i][j] = static_cast<char>((i * j + 73) % 256);

    int fd = fs->openFile(names[i].c_str(), true); // truncate
    assert(fd != -1);
    assert(fs->writeFile(fd, contents[i].data(), size) == size);
    assert(fs->closeFile(fd));
  }

  // Удаление файлов по одному и проверка оставшихся
  for (int toDelete = 0; toDelete < fileCount; ++toDelete) {
    assert(fs->deleteFile(names[toDelete].c_str()));

    for (int i = toDelete + 1; i < fileCount; ++i) {
      int fd = fs->openFile(names[i].c_str(), false);
      assert(fd != -1);

      std::vector<char> readBuf(contents[i].size());
      size_t read = fs->readFile(fd, readBuf.data(), readBuf.size());
      assert(read == contents[i].size());
      assert(fs->closeFile(fd));
      std::cout << readBuf[0] << readBuf[1] << readBuf[2] << readBuf[3] << readBuf[4] << readBuf[5] << "\n";
      std::cout << contents[i][0] << contents[i][1] << contents[i][2] << contents[i][3] << contents[i][4] << contents[i][5] << "\n";
      assert(memcmp(readBuf.data(), contents[i].data(), readBuf.size()) == 0);
    }
  }


  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("BIG FILES TEST FINISHED\n");
}

void testGiantFile() {
  const int fileCount = 1;
  const size_t maxSize = 1024 * 1024 * 512; // 512 mb

  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  uint8_t * buffer = static_cast<uint8_t *>(calloc(maxSize, sizeof(uint8_t)));
  memset(buffer, 9, maxSize);

  int fd = fs->openFile("BIGFILE", true);
  assert(fd != -1);
  size_t was_writed = fs->writeFile(fd, buffer, maxSize);
  std::cout << "Write bytes: " << was_writed << "\n";
  std::cout << maxSize << "\n";
  assert(was_writed == maxSize);
  assert(fs->closeFile(fd));


  uint8_t * buffer_out = static_cast<uint8_t *>(calloc(maxSize, sizeof(uint8_t)));

  fd = fs->openFile("BIGFILE", false);
  assert(fd != -1);

  size_t read = fs->readFile(fd, buffer_out, maxSize);
  assert(read == maxSize);
  assert(fs->closeFile(fd));
  assert(memcmp(buffer, buffer_out, maxSize) == 0);

  assert(fs->deleteFile("BIGFILE"));



  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("GIANT FILES TEST FINISHED\n");
  free(buffer);
}

// Функция тестирования операций над произвольными файлами с удалением
void testArbitraryFilesWithDelete() {
  const int fileCount = 1;
  const size_t maxSize = 16384; // до 16 KiB

  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  std::vector<std::string> names(fileCount);
  std::vector<std::vector<char>> contents(fileCount);

  // Генерация и запись файлов
  for (int i = 0; i < fileCount; ++i) {
    names[i] = "file_" + std::to_string(i);
    size_t size = 200 + (i * 791) % maxSize; // от 200 до ~16KiB
    contents[i].resize(size);

    for (size_t j = 0; j < size; ++j)
      contents[i][j] = static_cast<char>((i * j + 73) % 256);

    int fd = fs->openFile(names[i].c_str(), true); // truncate
    assert(fd != -1);
    assert(fs->writeFile(fd, contents[i].data(), size) == size);

    // Дозапись сразу, не закрывая дескриптор
    if (i % 3 == 0) {
      std::vector<char> extra(5000);
      for (size_t j = 0; j < extra.size(); ++j)
        extra[j] = static_cast<char>((i * j + 91) % 256);

      assert(fs->writeFile(fd, extra.data(), extra.size()) == extra.size());
      contents[i].insert(contents[i].end(), extra.begin(), extra.end());
    }

    assert(fs->closeFile(fd));
  }

  // Проверка содержимого всех файлов
  for (int i = 0; i < fileCount; ++i) {
    int fd = fs->openFile(names[i].c_str(), false);
    assert(fd != -1);

    std::vector<char> readBuf(contents[i].size());
    size_t read = fs->readFile(fd, readBuf.data(), readBuf.size());
    assert(read == contents[i].size());
    std::cout << readBuf[0] << readBuf[1] << readBuf[2] << readBuf[3] << "\n";
    std::cout << contents[i][0] <<  contents[i][1] << contents[i][2] << contents[i][3] <<"\n";
    assert(memcmp(readBuf.data(), contents[i].data(), readBuf.size()) == 0);
    assert(fs->closeFile(fd));
  }

  // Удаление файлов по одному и проверка оставшихся
  for (int toDelete = 0; toDelete < fileCount; ++toDelete) {
    assert(fs->deleteFile(names[toDelete].c_str()));

    for (int i = toDelete + 1; i < fileCount; ++i) {
      int fd = fs->openFile(names[i].c_str(), false);
      assert(fd != -1);

      std::vector<char> readBuf(contents[i].size());
      size_t read = fs->readFile(fd, readBuf.data(), readBuf.size());
      assert(read == contents[i].size());
      assert(memcmp(readBuf.data(), contents[i].data(), readBuf.size()) == 0);
      assert(fs->closeFile(fd));
    }
  }

  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("testArbitraryFilesWithDelete PASSED\n");
}


void testRelocationWrite() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  // Сначала запишем 1024 байта
  const size_t sz1 = 1024;
  const size_t sz2 = 1500; // будет релокация
  std::vector<char> data1(sz1, 'A');
  std::vector<char> data2(sz2, 'B');

  int fd = fs->openFile("relotest", true);
  assert(fd != -1);
  assert(fs->writeFile(fd, data1.data(), data1.size()) == data1.size());

  // Теперь дозапишем, должно быть перенесено
  assert(fs->writeFile(fd, data2.data(), data2.size()) == data2.size());
  assert(fs->closeFile(fd));

  // Проверим содержимое
  std::vector<char> readBuf(sz1 + sz2);
  fd = fs->openFile("relotest", false);
  assert(fd != -1);
  assert(fs->readFile(fd, readBuf.data(), readBuf.size()) == readBuf.size());
  assert(fs->closeFile(fd));

  for (size_t i = 0; i < sz1; ++i)
    assert(readBuf[i] == 'A');

  for (size_t i = sz1; i < sz1 + sz2; ++i)
    assert(readBuf[i] == 'B');

  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("Relocation + append test passed\n");
}

void testRelocationWriteMinimal() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  // Буфер 1024 байта
  std::vector<char> original(1024, 'A');
  for (size_t i = 0; i < original.size(); ++i)
    original[i] = static_cast<char>(i % 256);

  // Дозапись 1500 байт
  std::vector<char> append(1500, 'B');
  for (size_t i = 0; i < append.size(); ++i)
    append[i] = static_cast<char>((i + 17) % 256);

  // Создание файла
  int fd = fs->openFile("relocate", true);
  assert(fd != -1);
  assert(fs->writeFile(fd, original.data(), original.size()) == original.size());

  // Дозапись, должна вызвать релокацию
  assert(fs->writeFile(fd, append.data(), append.size()) == append.size());
  assert(fs->closeFile(fd));

  // Чтение и сравнение
  fd = fs->openFile("relocate", false);
  assert(fd != -1);

  std::vector<char> readBuf(original.size() + append.size());
  size_t actual = fs->readFile(fd, readBuf.data(), readBuf.size());
  std::cout << actual << std::endl;
  std::cout << readBuf.size() << std::endl;
  assert(actual == readBuf.size());

  // Проверка содержимого
  assert(memcmp(readBuf.data(), original.data(), original.size()) == 0);
  assert(memcmp(readBuf.data() + original.size(), append.data(), append.size()) == 0);

  assert(fs->closeFile(fd));
  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("testRelocationWrite PASSED\n");
}

void testRelocationWriteMiniCrash() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  char A[1024];
  for (int i = 0; i < 1024; ++i) A[i] = i % 256;

  char B[1500];
  for (int i = 0; i < 1500; ++i) B[i] = (i + 17) % 256;

  // записываем оригинальные 1024 байта
  int fd = fs->openFile("x", true);
  assert(fd != -1);
  assert(fs->writeFile(fd, A, 1024) == 1024);

  // записываем дополнительно 1500 байт (должна вызвать релокацию)
  assert(fs->writeFile(fd, B, 1500) == 1500);
  assert(fs->closeFile(fd));

  // читаем весь файл
  char buf[2524] = {};
  fd = fs->openFile("x", false);
  assert(fd != -1);
  size_t actual = fs->readFile(fd, buf, sizeof(buf));
  std::cout << "read: " << actual << ", expected: 2524\n";
  assert(actual == 2524);  // тут и падает

  // сверяем содержимое
  assert(memcmp(buf, A, 1024) == 0);
  assert(memcmp(buf + 1024, B, 1500) == 0);

  assert(fs->closeFile(fd));
  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("testRelocationWriteMiniCrash OK\n");
}

void testRelocationWriteMinimal_no_truncate() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  // Оригинальные данные (1024 байта)
  char original[1024];
  for (int i = 0; i < 1024; ++i)
    original[i] = static_cast<char>(i % 256);

  // Данные для дозаписи (1500 байт)
  char append[1500];
  for (int i = 0; i < 1500; ++i)
    append[i] = static_cast<char>((i + 17) % 256);

  // Открываем файл один раз на запись
  int fd = fs->openFile("relocate", true);
  assert(fd != -1);

  // Первая запись (1024 байта)
  assert(fs->writeFile(fd, original, sizeof(original)) == sizeof(original));

  // Дозапись, которая должна вызвать релокацию (1500 байт)
  assert(fs->writeFile(fd, append, sizeof(append)) == sizeof(append));
  assert(fs->closeFile(fd));

  // Открываем файл для чтения
  fd = fs->openFile("relocate", false);
  assert(fd != -1);

  // Ожидаем: 1024 + 1500 = 2524 байта
  char readBuf[2524];
  size_t actual = fs->readFile(fd, readBuf, sizeof(readBuf));
  std::cout << "read: " << actual << ", expected: " << sizeof(readBuf) << std::endl;
  assert(actual == sizeof(readBuf));

  // Проверяем содержимое
  assert(memcmp(readBuf, original, sizeof(original)) == 0);
  assert(memcmp(readBuf + sizeof(original), append, sizeof(append)) == 0);

  assert(fs->closeFile(fd));
  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("testRelocationWriteMinimal_no_truncate PASSED\n");
}

//-------------------------------------------------------------------------------------------------

void testForcedRelocationAndChunkedRead() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem* fs = CFileSystem::mount(dev);
  assert(fs);

  // Создаем файл блокер
  int fd1 = fs->openFile("first", true);
  assert(fd1 != -1);
  char original[500];
  for (int i = 0; i < 500; ++i)
    original[i] = static_cast<char>((i * 7) % 256);
  assert(fs->writeFile(fd1, original, 500) == 500);
  assert(fs->closeFile(fd1));

  // Второй
  int fd2 = fs->openFile("second", true);
  assert(fd2 != -1);
  char original2[500];
  for (int i = 0; i < 500; ++i)
    original2[i] = static_cast<char>((i * 70) % 256);
  assert(fs->writeFile(fd2, original, 500) == 500);
  assert(fs->closeFile(fd2));

  // Удаляем первый
  assert(fs->deleteFile("first"));

  // Создаем нормальный файл вначале
  int fd3 = fs->openFile("third", true);
  assert(fd3 != -1);
  char original3[1000];
  for (int i = 0; i < 1000; ++i)
    original3[i] = static_cast<char>((i * 12) % 256);
  assert(fs->writeFile(fd3, original3, 500) == 500);
  assert(fs->writeFile(fd3, original3 + 500, 500) == 500);
  assert(fs->closeFile(fd3));

  // Проверяем: читаем
  fd1 = fs->openFile("third", false);
  assert(fd1 != -1);

  char got[1000];
  assert(fs->readFile(fd1, got, 1000) == 1000);

  assert(memcmp(original3, got, 1000) == 0);
  assert(fs->closeFile(fd1));
  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("testForcedRelocationAndChunkedRead PASSED\n");
}

void testNoRelocationAndChunkedRead() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem* fs = CFileSystem::mount(dev);
  assert(fs);

  // Сначала запишем большой файл с 6000 байт сразу — чтобы сектора были подряд
  int fd = fs->openFile("bigfile", true);
  assert(fd != -1);

  char full[6000];
  for (int i = 0; i < 3000; ++i)
    full[i] = static_cast<char>((i * 7) % 256); // original часть

  for (int i = 0; i < 3000; ++i)
    full[3000 + i] = static_cast<char>((i * 5 + 13) % 256); // extra часть

  // Пишем всё сразу — никакой релокации не будет
  assert(fs->writeFile(fd, full, 6000) == 6000);
  assert(fs->writeFile(fd, full, 6000) == 6000);
  assert(fs->closeFile(fd));

  // Проверяем: читаем по кускам (chunked)
  fd = fs->openFile("bigfile", false);
  assert(fd != -1);

  char buf[12000];
  size_t total = 0;

  while (total < 12000) {
    size_t chunk = 1000;
    if (chunk > 6000 - total) chunk = 6000 - total;

    size_t actual = fs->readFile(fd, buf + total, chunk);
    assert(actual == chunk); // если readOffset работает — всё будет ок
    total += actual;
  }

  // Сравниваем
  std::cout << "REF: " << full[0] << full[1] << full[2] << full[3] << std::endl;
  std::cout << "BUF: " << buf[0] << buf[1] << buf[2] << buf[3] << std::endl;

  assert(memcmp(full, buf, 6000) == 0);
  assert(fs->closeFile(fd));
  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("testNoRelocationAndChunkedRead PASSED\n");
}

void testSimpleAppendAndCheck() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem* fs = CFileSystem::mount(dev);
  assert(fs);

  // Первоначальные 3000 байт
  char original[3000] = {'A'};

  // Дописываемые 3000 байт
  char extra[3000] = {'B'};

  // Эталонный буфер
  char ref[6000];
  memcpy(ref, original, 3000);
  memcpy(ref + 3000, extra, 3000);

  // Создание и запись
  int fd = fs->openFile("appendfile", true);
  assert(fd != -1);
  assert(fs->writeFile(fd, original, 3000) == 3000);
  assert(fs->writeFile(fd, extra, 3000) == 3000);
  assert(fs->closeFile(fd));

  // Читаем весь файл
  fd = fs->openFile("appendfile", false);
  assert(fd != -1);
  char buf[6000] = {};
  size_t actual = fs->readFile(fd, buf, 6000);
  std::cout << actual << std::endl;
  assert(actual == 6000);

  // Сравниваем
  for (int i = 0; i < 3000 * 2; ++i) {
    printf("%c", ref[i]);
  }
  printf("\n");

  for (int i = 0; i < 3000 * 2; ++i) {
    printf("%c", buf[i]);
  }
  printf("\n");


  assert(memcmp(ref, buf, 6000) == 0);

  assert(fs->closeFile(fd));
  assert(fs->umount());
  delete fs;
  doneDisk();

  printf("testSimpleAppendAndCheck PASSED\n");
}

void testFullFilesystemStress() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  const int fileCount = DIR_ENTRIES_MAX;
  const size_t baseSize = 8192; // 8 KiB
  std::vector<std::string> names(fileCount);
  std::vector<std::vector<char>> contents(fileCount);

  // === Создание и запись ===
  for (int i = 0; i < fileCount; ++i) {
    names[i] = "file_" + std::to_string(i);
    contents[i].resize(baseSize + (i % 5) * 4096); // от 8 до 28 KiB
    for (size_t j = 0; j < contents[i].size(); ++j)
      contents[i][j] = static_cast<char>((i * j + 17) % 256);

    int fd = fs->openFile(names[i].c_str(), true);
    assert(fd != -1);
    assert(fs->writeFile(fd, contents[i].data(), contents[i].size()) == contents[i].size());

    // Дозапись сразу после основного блока
    if (i % 3 == 0) {
      std::vector<char> extra(4096, static_cast<char>(i));
      assert(fs->writeFile(fd, extra.data(), extra.size()) == extra.size());
      contents[i].insert(contents[i].end(), extra.begin(), extra.end());
    }

    assert(fs->closeFile(fd));
  }

  // === Проверка ===
  for (int i = 0; i < fileCount; ++i) {
    int fd = fs->openFile(names[i].c_str(), false);
    assert(fd != -1);
    std::vector<char> buffer(contents[i].size());
    size_t read = fs->readFile(fd, buffer.data(), buffer.size());
    assert(read == buffer.size());
    assert(memcmp(buffer.data(), contents[i].data(), buffer.size()) == 0);
    assert(fs->closeFile(fd));
  }

  // === Удаление половины и повторное использование ===
  for (int i = 0; i < fileCount; i += 2)
    assert(fs->deleteFile(names[i].c_str()));

  // === Запись в освободившиеся слоты ===
  for (int i = 0; i < fileCount; i += 2) {
    std::string name = "new_" + std::to_string(i);
    std::vector<char> data(16384, static_cast<char>((i + 42) % 256));

    int fd = fs->openFile(name.c_str(), true);
    assert(fd != -1);
    assert(fs->writeFile(fd, data.data(), data.size()) == data.size());
    assert(fs->closeFile(fd));

    // Проверка
    fd = fs->openFile(name.c_str(), false);
    assert(fd != -1);
    std::vector<char> buffer(data.size());
    assert(fs->readFile(fd, buffer.data(), buffer.size()) == buffer.size());
    assert(memcmp(buffer.data(), data.data(), buffer.size()) == 0);
    assert(fs->closeFile(fd));
  }

  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("testFullFilesystemStress PASSED\n");
}



#include <unordered_map>

void testFullStressBigFiles() {
  constexpr size_t MAX_FILE_COUNT = 128;
  constexpr size_t MAX_FILE_SIZE = 10 * 1024 * 1024;
  constexpr size_t ITERATIONS = 500;

  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  struct FileEntry {
    std::string name;
    std::vector<uint8_t> data;
    int fd = -1;
  };

  std::vector<FileEntry> files;
  std::unordered_map<std::string, int> name_to_index;

  // Открываем до MAX_FILE_COUNT файлов
  for (size_t i = 0; i < 8; ++i) {
    FileEntry f;
    f.name = "file_" + std::to_string(i);
    f.fd = fs->openFile(f.name.c_str(), true);
    assert(f.fd != -1);
    files.push_back(std::move(f));
    name_to_index[files.back().name] = i;
  }

  // Основной стресс-тест
  for (size_t iter = 0; iter < ITERATIONS; ++iter) {
    // случайный файл
    size_t fileIndex = rand() % files.size();
    FileEntry &f = files[fileIndex];

    // случайный размер записи
    size_t chunk = 1 + rand() % MAX_FILE_SIZE;

    std::vector<uint8_t> data(chunk);
    for (size_t i = 0; i < chunk; ++i)
      data[i] = static_cast<uint8_t>((iter + i) % 256);

    // сохраняем offset
    size_t offsetBefore = f.data.size();

    // дописываем в память
    f.data.insert(f.data.end(), data.begin(), data.end());

    // записываем в файл
    size_t written = fs->writeFile(f.fd, data.data(), data.size());
    assert(written == data.size());

    // случайно читаем и проверяем старые файлы
    if (iter % 25 == 0 && !f.data.empty()) {
      int checkIndex = rand() % files.size();
      FileEntry &fc = files[checkIndex];

      fs->closeFile(fc.fd);
      fc.fd = fs->openFile(fc.name.c_str(), false);
      assert(fc.fd != -1);

      std::vector<uint8_t> read(fc.data.size(), 0);
      size_t r = fs->readFile(fc.fd, read.data(), read.size());

      printf("READ: fd %d, readOffset %zu, fileSize %zu, requested %zu, actual %zu\n",
             fc.fd, r, fc.data.size(), read.size(), r);

      assert(r == fc.data.size());
      assert(memcmp(fc.data.data(), read.data(), r) == 0);

      fs->closeFile(fc.fd);
      fc.fd = fs->openFile(fc.name.c_str(), true);
    }

    // случайное удаление и создание нового файла
    if (iter % 50 == 0 && files.size() >= MAX_FILE_COUNT) {
      size_t killIndex = rand() % files.size();
      fs->closeFile(files[killIndex].fd);
      assert(fs->deleteFile(files[killIndex].name.c_str()));

      FileEntry fnew;
      fnew.name = "file_" + std::to_string(iter) + "_new";
      fnew.fd = fs->openFile(fnew.name.c_str(), true);
      assert(fnew.fd != -1);
      files[killIndex] = std::move(fnew);
    }

    if (iter % 25 == 0)
      printf("Iteration %zu done\n", iter);
  }

  // Закрытие всех файлов
  for (auto &f : files) {
    if (f.fd != -1) fs->closeFile(f.fd);
  }

  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("testFullStressBigFiles PASSED\n");
}

static void testMountPersistence()
{
  const char *fileName = "persistent_file";
  const char *fileContent = "This file should survive a reboot!";
  char readBuffer[128] = {0};
  TFile fileInfo;

  printf("\n========== [DEBUG] testMountPersistence() started ==========\n");

  // === FORMAT DISK ===
  printf("[DEBUG] Formatting disk...\n");
  assert(CFileSystem::createFs(createDisk()));
  doneDisk();
  printf("[DEBUG] Disk formatted and closed.\n");

  // === 1. Записываем файл ===
  {
    printf("[DEBUG] Mounting disk for write...\n");
    CFileSystem *fs = CFileSystem::mount(openDisk());
    assert(fs);

    printf("[DEBUG] Creating file: '%s'\n", fileName);
    int fd = fs->openFile(fileName, true);
    assert(fd != -1);
    printf("[DEBUG] File descriptor: %d\n", fd);

    size_t expectedWriteSize = strlen(fileContent);
    printf("[DEBUG] Writing %zu bytes...\n", expectedWriteSize);
    size_t written = fs->writeFile(fd, fileContent, expectedWriteSize);
    printf("[DEBUG] Bytes written: %zu\n", written);
    assert(written == expectedWriteSize);

    assert(fs->closeFile(fd));

    // 🔍 ДО UMOUNT: проверим, что данные читаются прямо сейчас
    fd = fs->openFile(fileName, false);
    assert(fd != -1);
    size_t bytesRead = fs->readFile(fd, readBuffer, sizeof(readBuffer) - 1);
    readBuffer[bytesRead] = '\0';
    printf("[DEBUG] PRE-UMOUNT Read content: '%s'\n", readBuffer);
    printf("[DEBUG] PRE-UMOUNT Expected: '%s'\n", fileContent);
    assert(strcmp(readBuffer, fileContent) == 0);
    assert(fs->closeFile(fd));

    printf("[DEBUG] Umounting file system...\n");
    assert(fs->umount());
    delete fs;
    doneDisk(); // Симулируем выключение
  }

  // === 2. После перезагрузки проверяем наличие и содержимое ===
  {
    printf("[DEBUG] Mounting disk after reboot...\n");
    CFileSystem *fs = CFileSystem::mount(openDisk());
    assert(fs);

    bool found = fs->findFirst(fileInfo);
    assert(found);

    printf("[DEBUG] Found file name: '%s'\n", fileInfo.m_FileName);
    printf("[DEBUG] File size: %zu\n", fileInfo.m_FileSize);
    assert(strcmp(fileInfo.m_FileName, fileName) == 0);
    assert(fileInfo.m_FileSize == strlen(fileContent));

    int fd = fs->openFile(fileInfo.m_FileName, false);
    assert(fd != -1);

    size_t bytesRead = fs->readFile(fd, readBuffer, sizeof(readBuffer) - 1);
    readBuffer[bytesRead] = '\0'; // null-terminate

    printf("[DEBUG] Read content: '%s'\n", readBuffer);
    printf("[DEBUG] Expected: '%s'\n", fileContent);
    printf("[DEBUG] Size expected: %zu, got: %zu\n", strlen(fileContent), bytesRead);

    assert(strcmp(readBuffer, fileContent) == 0);
    assert(fs->closeFile(fd));

    assert(fs->umount());
    delete fs;
    doneDisk(); // Завершили
  }

  printf("[DEBUG] testMountPersistence() ✅ SUCCESS ✅\n");
}

#include <vector>
#include <random>
#include <cassert>
#include <cstring>

static void testMismatchOnRandomSizedFiles() {
  constexpr size_t FILE_COUNT = 2;
  constexpr size_t MAX_FILE_SIZE = 1024; // до 4 MiB на файл
  constexpr size_t WRITES = 2;

  std::mt19937 rng(42);
  std::uniform_int_distribution<size_t> sizeGen(1, MAX_FILE_SIZE);
  std::uniform_int_distribution<size_t> filePick(0, FILE_COUNT - 1);

  // создаём диск и ФС
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  struct File {
    std::string name;
    std::vector<uint8_t> expected;
    int fd = -1;
  };

  std::vector<File> files(FILE_COUNT);
  for (size_t i = 0; i < FILE_COUNT; ++i) {
    files[i].name = "file_" + std::to_string(i);
    files[i].fd = fs->openFile(files[i].name.c_str(), true);
    assert(files[i].fd != -1);
  }
  std::cout << "[DEBUG] Writing files...\n";

  // записываем случайные данные по частям
  for (size_t w = 0; w < WRITES; ++w) {
    for (size_t i = 0; i < FILE_COUNT; ++i) {
      size_t chunk = sizeGen(rng);
      std::vector<uint8_t> data(chunk);
      for (size_t j = 0; j < chunk; ++j)
        data[j] = static_cast<uint8_t>((i + j + w) % 256);
      printf("[TEST] write to %s: data.size() = %zu, expected.size() before = %zu\n",
             files[i].name.c_str(), data.size(), files[i].expected.size());
      size_t written = fs->writeFile(files[i].fd, data.data(), data.size());
      assert(written == chunk);
      files[i].expected.insert(files[i].expected.end(), data.begin(), data.end());
      printf("[TEST] expected.size() after = %zu\n", files[i].expected.size());
    }
  }

 std::cout << "[DEBUG] Writing files...\n";

  // закрываем всё и монтируем заново (имитация перезагрузки)
  for (auto &f : files) {
    assert(fs->closeFile(f.fd));
  }
  assert(fs->umount());
  delete fs;

  // снова монтируем
  fs = CFileSystem::mount(openDisk());
  assert(fs);

  // проверка всех файлов
  for (auto &f : files) {
    int fd = fs->openFile(f.name.c_str(), false);
    assert(fd != -1);

    size_t fSize = fs->fileSize(f.name.c_str());
    assert(fSize == f.expected.size()); // 🔥 критичная проверка!

    std::vector<uint8_t> buffer(fSize);
    size_t read = fs->readFile(fd, buffer.data(), buffer.size());
    assert(read == fSize); // 🔥 критичная проверка!
    assert(memcmp(buffer.data(), f.expected.data(), fSize) == 0);

    assert(fs->closeFile(fd));
  }

  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("testMismatchOnRandomSizedFiles PASSED\n");
}

void testEBEISHIYPROSTOPIZDAYAVAHUE() {
  try {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  std::vector<std::string> names(10);
  for (int i = 0; i < 10; ++i)
    names[i] = "file_" + std::to_string(i);

  // Создаём и записываем 1 файл (100 МБ)
  for (int i = 0; i < 10; i++) {
    uint8_t *buffer = static_cast<uint8_t *>(calloc(100 * 1024 * 1024, 1)); // 100 MB
    for (int j = 0; j < 100 * 1024 * 1024; ++j)
      buffer[j] = static_cast<uint8_t>(j % 256);

    int fd = fs->openFile(names[i].c_str(), true);
    assert(fd != -1);
    assert(fs->writeFile(fd, buffer, 100 * 1024 * 1024) == 100 * 1024 * 1024);
    assert(fs->closeFile(fd));
    free(buffer);
  }

  // Читаем и проверяем
  for (int i = 0; i < 10; i++) {
    uint8_t *buffer_read = static_cast<uint8_t *>(calloc(100 * 1024 * 1024, 1)); // 100 MB
    uint8_t *buffer_ref = static_cast<uint8_t *>(calloc(100 * 1024 * 1024, 1));  // 100 MB
    for (int j = 0; j < 100 * 1024 * 1024; ++j)
      buffer_ref[j] = static_cast<uint8_t>(j % 256);

    int fd = fs->openFile(names[i].c_str(), false);
    assert(fd != -1);
    size_t read = fs->readFile(fd, buffer_read, 100 * 1024 * 1024);
    assert(read == 100 * 1024 * 1024);
    assert(memcmp(buffer_read, buffer_ref, 100 * 1024 * 1024) == 0);
    assert(fs->closeFile(fd));

    free(buffer_read);
    free(buffer_ref);
  }

    // Удаляем
    for (int i = 0; i < 5; i++) {
      assert(fs->deleteFile(names[i].c_str()));
    }

    // Читаем и проверяем
    for (int i = 5; i < 10; i++) {
      uint8_t *buffer_read = static_cast<uint8_t *>(calloc(100 * 1024 * 1024, 1)); // 100 MB
      uint8_t *buffer_ref = static_cast<uint8_t *>(calloc(100 * 1024 * 1024, 1));  // 100 MB
      for (int j = 0; j < 100 * 1024 * 1024; ++j)
        buffer_ref[j] = static_cast<uint8_t>(j % 256);

      int fd = fs->openFile(names[i].c_str(), false);
      assert(fd != -1);
      size_t read = fs->readFile(fd, buffer_read, 100 * 1024 * 1024);
      assert(read == 100 * 1024 * 1024);
      assert(memcmp(buffer_read, buffer_ref, 100 * 1024 * 1024) == 0);
      assert(fs->closeFile(fd));

      free(buffer_read);
      free(buffer_ref);
    }

    //созздаем файл и пищем в него дохуя
    uint8_t * buffer = static_cast<uint8_t *>(calloc(100 * 1024 * 1024, 1)); // 100 MB
    int fd = fs->openFile("BOLSHOY", true);
    assert(fd != -1);

    for (int i = 0; i < 2; i++) {
      size_t writed = fs->writeFile(fd, buffer, 100 * 1024 * 1024);
      std::cout << "WRITED: " << writed << std::endl;
      std::cout << "WRITTEN: " << 100 * 1024 * 1024 << std::endl;
      assert(writed == 100 * 1024 * 1024);
    }
    assert(fs->closeFile(fd));

    // Сверяемся
    int fd11 = fs->openFile("BOLSHOY", false);
    assert(fd11 != -1);

    uint8_t * buffer_in = static_cast<uint8_t *>(calloc(100 * 1024 * 1024 * 2, 1)); // 100 MB
    uint8_t * buffer_out = static_cast<uint8_t *>(calloc(100 * 1024 * 1024 * 2, 1)); // 100 MB


    size_t read = fs->readFile(fd, buffer_in, 100 * 1024 * 1024 * 2);
    assert(read == 100 * 1024 * 1024 * 2);

    assert(memcmp(buffer_in, buffer_out, 100 * 1024 * 1024 * 2) == 0);
    assert(fs->closeFile(fd));

    free(buffer_in);
    free(buffer_out);

  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("testEBEISHIYPROSTOPIZDAYAVAHUE PASSED\n");
} catch (const char *msg) {
  printf("!!! EXCEPTION: %s\n", msg);
  assert(false);
}
}

#include <algorithm>

void testFindNextInconsistency() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  // Создаём 3 файла
  const char *names[] = {"A", "B", "C"};
  for (int i = 0; i < 3; ++i) {
    int fd = fs->openFile(names[i], true);
    assert(fd != -1);
    uint8_t data[3] = {static_cast<uint8_t>('a' + i)};
    assert(fs->writeFile(fd, data, 1) == 1);
    assert(fs->closeFile(fd));
  }

  // Удаляем файл "B"
  assert(fs->deleteFile("B"));

  // Теперь проверим findFirst / findNext
  TFile f{};
  std::vector<std::string> foundNames;

  if (fs->findFirst(f)) {
    foundNames.emplace_back(f.m_FileName);
    while (fs->findNext(f)) {
      foundNames.emplace_back(f.m_FileName);
    }
  }

  std::cout << "[DEBUG] Files found via findFirst/findNext:\n";
  for (const auto &name : foundNames) {
    std::cout << "- " << name << '\n';
  }

  assert(foundNames.size() == 2);
  assert(std::find(foundNames.begin(), foundNames.end(), "B") == foundNames.end());

  // Проверим повторное создание файла с тем же именем
  int fd = fs->openFile("B", true);
  assert(fd != -1);
  uint8_t z = 'z';
  assert(fs->writeFile(fd, &z, 1) == 1);
  assert(fs->closeFile(fd));

  // Повторный обход
  foundNames.clear();
  if (fs->findFirst(f)) {
    foundNames.emplace_back(f.m_FileName);
    while (fs->findNext(f)) {
      foundNames.emplace_back(f.m_FileName);
    }
  }

  std::cout << "[DEBUG] After re-creating file 'B':\n";
  for (const auto &name : foundNames) {
    std::cout << "- " << name << '\n';
  }

  assert(foundNames.size() == 3);
  assert(std::find(foundNames.begin(), foundNames.end(), "B") != foundNames.end());

  assert(fs->umount());
  delete fs;
  doneDisk();
  std::cout << "[TEST PASSED] testFindNextInconsistency\n";
}

void testComplexFileOperations() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  constexpr size_t FILE_COUNT = 16;
  constexpr size_t MAX_CHUNK = 1024 * 32;
  std::vector<std::string> names;
  for (size_t i = 0; i < FILE_COUNT; ++i)
    names.emplace_back("file_" + std::to_string(i));

  std::vector<std::vector<uint8_t>> contents(FILE_COUNT);

  std::mt19937 rng(1337);
  std::uniform_int_distribution<size_t> chunkGen(512, MAX_CHUNK);

  // === 1. Первая волна записи ===
  for (size_t i = 0; i < FILE_COUNT; ++i) {
    int fd = fs->openFile(names[i].c_str(), true);
    assert(fd != -1);
    size_t total = 0;
    for (int w = 0; w < 3; ++w) {
      size_t chunk = chunkGen(rng);
      std::vector<uint8_t> buf(chunk);
      for (size_t j = 0; j < chunk; ++j)
        buf[j] = static_cast<uint8_t>((i + j + w) % 256);

      size_t written = fs->writeFile(fd, buf.data(), buf.size());
      assert(written == chunk);
      contents[i].insert(contents[i].end(), buf.begin(), buf.end());
      total += written;
    }
    assert(fs->closeFile(fd));
  }

  // === 2. Проверка чтения ===
  for (size_t i = 0; i < FILE_COUNT; ++i) {
    int fd = fs->openFile(names[i].c_str(), false);
    assert(fd != -1);
    size_t expectedSize = contents[i].size();
    std::vector<uint8_t> buffer(expectedSize);
    size_t read = fs->readFile(fd, buffer.data(), buffer.size());
    assert(read == expectedSize);
    assert(memcmp(buffer.data(), contents[i].data(), expectedSize) == 0);
    assert(fs->closeFile(fd));
  }

  // === 3. Удаление половины файлов ===
  for (size_t i = 0; i < FILE_COUNT; i += 2) {
    assert(fs->deleteFile(names[i].c_str()));
  }

  // === 4. Создание новых файлов в удалённые слоты ===
  for (size_t i = 0; i < FILE_COUNT; i += 2) {
    std::string name = "new_file_" + std::to_string(i);
    int fd = fs->openFile(name.c_str(), true);
    assert(fd != -1);

    std::vector<uint8_t> buf(2048, static_cast<uint8_t>(i));
    size_t written = fs->writeFile(fd, buf.data(), buf.size());
    assert(written == 2048);
    assert(fs->closeFile(fd));

    // Проверка чтения
    fd = fs->openFile(name.c_str(), false);
    assert(fd != -1);
    std::vector<uint8_t> check(2048);
    size_t read = fs->readFile(fd, check.data(), check.size());
    assert(read == 2048);
    for (size_t j = 0; j < 2048; ++j)
      assert(check[j] == static_cast<uint8_t>(i));
    assert(fs->closeFile(fd));
  }

  assert(fs->umount());
  delete fs;
  doneDisk();
  std::cout << "✅ testComplexFileOperations PASSED\n";
}

void testStressLargeFilesComplex() {
  constexpr size_t FILE_COUNT = 8;
  constexpr size_t MAX_FILE_SIZE = 10 * 1024 * 1024;
  constexpr size_t WRITES = 4;

  std::mt19937 rng(42);
  std::uniform_int_distribution<size_t> sizeGen(1024 * 1024, MAX_FILE_SIZE); // от 128КБ до 5МБ

  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  struct File {
    std::string name;
    std::vector<uint8_t> expected;
    int fd = -1;
  };

  std::vector<File> files(FILE_COUNT);
  for (size_t i = 0; i < FILE_COUNT; ++i) {
    files[i].name = "bigfile_" + std::to_string(i);
    files[i].fd = fs->openFile(files[i].name.c_str(), true);
    assert(files[i].fd != -1);
  }

  // Пишем в каждый файл несколько больших кусков
  for (size_t w = 0; w < WRITES; ++w) {
    for (size_t i = 0; i < FILE_COUNT; ++i) {
      std::cout << "WRITE: " << w << std::endl;
      std::cout << "FILE_COUNT: " << i << std::endl;
      size_t chunk = sizeGen(rng);
      std::vector<uint8_t> data(chunk);
      for (size_t j = 0; j < chunk; ++j)
        data[j] = static_cast<uint8_t>((i + j + w) % 256);
      size_t written = fs->writeFile(files[i].fd, data.data(), chunk);
      assert(written == chunk);
      files[i].expected.insert(files[i].expected.end(), data.begin(), data.end());
    }
  }

  for (auto &f : files) {
    printf("[test] Closing file '%s' fd=%d\n", f.name.c_str(), f.fd);
    bool ok = fs->closeFile(f.fd);
    printf("[test] closeFile returned %d\n", ok);
    assert(ok);
  }

  assert(fs->umount());
  delete fs;

  // Перемонтируем и проверим
  fs = CFileSystem::mount(openDisk());
  assert(fs);

  // Проверка всех файлов
  for (auto &f : files) {
    int fd = fs->openFile(f.name.c_str(), false);
    assert(fd != -1);

    size_t fSize = fs->fileSize(f.name.c_str());
    if (fSize != f.expected.size()) {
      std::cout << "❌ SIZE MISMATCH on file: " << f.name << "\n";
      std::cout << "Expected size: " << f.expected.size() << "\n";
      std::cout << "Reported fileSize(): " << fSize << "\n";
      std::cout << "Difference: " << (long long)(fSize) - (long long)(f.expected.size()) << "\n";

      int fd_dbg = fs->openFile(f.name.c_str(), false);
      assert(fd_dbg != -1);
      size_t size_dbg = fs->fileSize(f.name.c_str());
      std::cout << "fileSize() inside check: " << size_dbg << "\n";

      std::vector<uint8_t> real_data(size_dbg);
      size_t actually_read = fs->readFile(fd_dbg, real_data.data(), size_dbg);
      std::cout << "Actually read: " << actually_read << "\n";

      if (actually_read != size_dbg) {
        std::cout << "!!! readFile returned less bytes than fileSize reports!\n";
      }

      assert(fs->closeFile(fd_dbg));
    }
    assert(fSize == f.expected.size());

    std::vector<uint8_t> buffer(fSize);
    size_t read = fs->readFile(fd, buffer.data(), fSize);
    assert(read == fSize);
    assert(memcmp(buffer.data(), f.expected.data(), fSize) == 0);

    assert(fs->closeFile(fd));
  }

  // Удалим часть файлов
  for (size_t i = 0; i < FILE_COUNT / 2; ++i)
    assert(fs->deleteFile(files[i].name.c_str()));

  // Проверим оставшиеся
  for (size_t i = FILE_COUNT / 2; i < FILE_COUNT; ++i) {
    auto &f = files[i];
    int fd = fs->openFile(f.name.c_str(), false);
    assert(fd != -1);
    size_t fSize = fs->fileSize(f.name.c_str());
    assert(fSize == f.expected.size());
    std::vector<uint8_t> buffer(fSize);
    size_t read = fs->readFile(fd, buffer.data(), fSize);
    assert(read == fSize);
    assert(memcmp(buffer.data(), f.expected.data(), fSize) == 0);
    assert(fs->closeFile(fd));
  }

  // Попробуем дописать в оставшиеся
  for (size_t i = FILE_COUNT / 2; i < FILE_COUNT; ++i) {
    auto &f = files[i];
    int fd = fs->openFile(f.name.c_str(), true);
    assert(fd != -1);
    std::vector<uint8_t> extra(512 * 1024, 123); // 512 КБ
    size_t written = fs->writeFile(fd, extra.data(), extra.size());
    assert(written == extra.size());
    f.expected.insert(f.expected.end(), extra.begin(), extra.end());
    assert(fs->closeFile(fd));
  }

  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("testStressLargeFilesComplex PASSED\n");
}

void testOtvalPizdy() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  int fd = fs->openFile("small", true);
  assert(fd != -1);
  fs->closeFile(fd);
  assert(fs->deleteFile("small"));


  uint8_t * data = static_cast<uint8_t *>(calloc(1024000000, sizeof(uint8_t)));
  fd = fs->openFile("big", true);
  assert(fd != -1);
  size_t written = fs->writeFile(fd, data, 2000000);
  fs->closeFile(fd);

  size_t size = fs->fileSize("big");
  assert(fs->fileSize("big") == size);


  assert(fs->deleteFile("big"));
  assert(fs->fileSize("big") == -1);

// ------------------------------------------------------------------------------------------

  constexpr size_t FILE_COUNT = 8;
  constexpr size_t MAX_FILE_SIZE = 100 * 1024 * 1024;
  constexpr size_t WRITES = 2;
  std::mt19937 rng(42);
  std::uniform_int_distribution<size_t> sizeGen(128 * 1024, MAX_FILE_SIZE);

  struct File {
    std::string name;
    std::vector<uint8_t> expected;
    int fd = -1;
  };


  std::vector<File> files(FILE_COUNT);
  for (size_t i = 0; i < FILE_COUNT; ++i) {
    files[i].name = "bigfile_" + std::to_string(i);
    files[i].fd = fs->openFile(files[i].name.c_str(), true);
    assert(files[i].fd != -1);
  }

  for (size_t w = 0; w < WRITES; ++w) {
    for (size_t i = 0; i < FILE_COUNT; ++i) {
      std::cout << "WRITE: " << w << std::endl;
      std::cout << "FILE_COUNT: " << i << std::endl;
      size_t chunk = sizeGen(rng);
      std::vector<uint8_t> data(chunk);
      for (size_t j = 0; j < chunk; ++j)
        data[j] = static_cast<uint8_t>((i + j + w) % 256);
      size_t written = fs->writeFile(files[i].fd, data.data(), chunk);
      assert(written == chunk);
      files[i].expected.insert(files[i].expected.end(), data.begin(), data.end());
    }
  }

  for (auto &f : files)
    assert(fs->closeFile(f.fd));

  // Проверка всех файлов
  for (auto &f : files) {
    int fd = fs->openFile(f.name.c_str(), false);
    assert(fd != -1);

    size_t fSize = fs->fileSize(f.name.c_str());
    if (fSize != f.expected.size()) {
      std::cout << "❌ SIZE MISMATCH on file: " << f.name << "\n";
      std::cout << "Expected size: " << f.expected.size() << "\n";
      std::cout << "Reported fileSize(): " << fSize << "\n";
      std::cout << "Difference: " << (long long)(fSize) - (long long)(f.expected.size()) << "\n";

      int fd_dbg = fs->openFile(f.name.c_str(), false);
      assert(fd_dbg != -1);
      size_t size_dbg = fs->fileSize(f.name.c_str());
      std::cout << "fileSize() inside check: " << size_dbg << "\n";

      std::vector<uint8_t> real_data(size_dbg);
      size_t actually_read = fs->readFile(fd_dbg, real_data.data(), size_dbg);
      std::cout << "Actually read: " << actually_read << "\n";

      if (actually_read != size_dbg) {
        std::cout << "!!! readFile returned less bytes than fileSize reports!\n";
      }

      assert(fs->closeFile(fd_dbg));
    }
    assert(fSize == f.expected.size());

    std::vector<uint8_t> buffer(fSize);
    size_t read = fs->readFile(fd, buffer.data(), fSize);
    assert(read == fSize);
    assert(memcmp(buffer.data(), f.expected.data(), fSize) == 0);

    assert(fs->closeFile(fd));
  }

  // Удалим часть файлов
  for (size_t i = 0; i < FILE_COUNT / 2; ++i)
    assert(fs->deleteFile(files[i].name.c_str()));


  // Проверим оставшиеся
  for (size_t i = FILE_COUNT / 2; i < FILE_COUNT; ++i) {
    auto &f = files[i];
    int fd = fs->openFile(f.name.c_str(), false);
    assert(fd != -1);
    size_t fSize = fs->fileSize(f.name.c_str());
    assert(fSize == f.expected.size());
    std::vector<uint8_t> buffer(fSize);
    size_t read = fs->readFile(fd, buffer.data(), fSize);
    assert(read == fSize);
    assert(memcmp(buffer.data(), f.expected.data(), fSize) == 0);
    assert(fs->closeFile(fd));
  }

  // Попробуем дописать в оставшиеся
  for (size_t i = FILE_COUNT / 2; i < FILE_COUNT; ++i) {
    auto &f = files[i];
    int fd = fs->openFile(f.name.c_str(), true);
    assert(fd != -1);
    std::vector<uint8_t> extra(512 * 1024, 123); // 512 КБ
    size_t written = fs->writeFile(fd, extra.data(), extra.size());
    assert(written == extra.size());
    f.expected.insert(f.expected.end(), extra.begin(), extra.end());
    assert(fs->closeFile(fd));
  }

  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("testOtvalPizdy PASSED\n");
}

#include <set>

static void testFindAfterDeletions() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  const std::vector<std::string> filenames = {
    "alpha", "beta", "gamma", "delta", "epsilon"
  };

  // Создаём и записываем файлы
  for (const std::string &name : filenames) {
    int fd = fs->openFile(name.c_str(), true);
    assert(fd != -1);
    std::vector<uint8_t> data(name.begin(), name.end());
    assert(fs->writeFile(fd, data.data(), data.size()) == data.size());
    assert(fs->writeFile(fd, data.data(), data.size()) == data.size());
    assert(fs->writeFile(fd, data.data(), data.size()) == data.size());
    assert(fs->closeFile(fd));
  }

  // Удаляем некоторые
  assert(fs->deleteFile("beta"));
  assert(fs->deleteFile("delta"));

  // Ожидаемые оставшиеся файлы
  std::set<std::string> expectedRemaining = { "alpha", "gamma", "epsilon" };
  std::set<std::string> actuallyFound;

  TFile entry;
  if (fs->findFirst(entry)) {
    actuallyFound.insert(std::string(entry.m_FileName));
    while (fs->findNext(entry)) {
      actuallyFound.insert(std::string(entry.m_FileName));
    }
  }

  assert(expectedRemaining == actuallyFound);

  assert(fs->umount());
  delete fs;
  doneDisk();

  printf("testFindAfterDeletions PASSED\n");
}

#include <map>

static void testFindAfterDeletionsWithContentCheck() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  const std::vector<std::string> filenames = {
    "alpha", "beta", "gamma", "delta", "epsilon"
  };

  // Храним ожидаемые данные
  std::map<std::string, std::vector<uint8_t>> expectedContent;

  // Создаём и дописываем данные
  for (const std::string &name : filenames) {
    int fd = fs->openFile(name.c_str(), true);
    assert(fd != -1);

    std::vector<uint8_t> firstChunk(name.begin(), name.end()); // первая запись
    std::vector<uint8_t> secondChunk(name.rbegin(), name.rend()); // вторая запись

    assert(fs->writeFile(fd, firstChunk.data(), firstChunk.size()) == firstChunk.size());
    assert(fs->writeFile(fd, secondChunk.data(), secondChunk.size()) == secondChunk.size());
    assert(fs->closeFile(fd));

    // сохраняем ожидаемые данные
    std::vector<uint8_t> fullContent = firstChunk;
    fullContent.insert(fullContent.end(), secondChunk.begin(), secondChunk.end());
    expectedContent[name] = std::move(fullContent);
  }

  // Удаляем некоторые
  assert(fs->deleteFile("beta"));
  assert(fs->deleteFile("delta"));

  // Ожидаемые оставшиеся
  std::set<std::string> expectedRemaining = { "alpha", "gamma", "epsilon" };
  std::set<std::string> actuallyFound;

  TFile entry;
  if (fs->findFirst(entry)) {
    actuallyFound.insert(entry.m_FileName);
    while (fs->findNext(entry)) {
      actuallyFound.insert(entry.m_FileName);
    }
  }

  assert(expectedRemaining == actuallyFound);

  // Проверка содержимого оставшихся файлов
  for (const std::string &name : expectedRemaining) {
    int fd = fs->openFile(name.c_str(), false);
    assert(fd != -1);

    const std::vector<uint8_t> &expected = expectedContent[name];
    std::vector<uint8_t> buffer(expected.size());

    size_t read = fs->readFile(fd, buffer.data(), buffer.size());
    assert(read == expected.size());
    assert(memcmp(buffer.data(), expected.data(), expected.size()) == 0);
    assert(fs->closeFile(fd));
  }

  assert(fs->umount());
  delete fs;
  doneDisk();

  printf("testFindAfterDeletionsWithContentCheck PASSED\n");
}

static void testUltraComplexFileSystemOperations() {
  constexpr size_t FILE_COUNT = 6;
  constexpr size_t CHUNK_SIZE = 512 * 1024; // 512 KB
  constexpr size_t EXTRA_WRITE = 256 * 1024; // +256 KB дополнительно
  const std::vector<std::string> filenames = {
    "orange", "blue_file", "gammaX", "important_data", "file_dead", "alpha_test"
  };
  std::set<std::string> deletedFiles = { "file_dead", "gammaX" };

  std::map<std::string, std::vector<uint8_t>> expectedContent;

  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  // 1. Создание файлов и начальная запись
  for (const std::string &name : filenames) {
    int fd = fs->openFile(name.c_str(), true);
    assert(fd != -1);

    std::vector<uint8_t> data(CHUNK_SIZE);
    for (size_t i = 0; i < data.size(); ++i)
      data[i] = static_cast<uint8_t>((i + name[0]) % 256);

    assert(fs->writeFile(fd, data.data(), data.size()) == data.size());

    // дополнительно дописываем EXTRA_WRITE байт
    std::vector<uint8_t> extra(EXTRA_WRITE, static_cast<uint8_t>(name.length()));
    assert(fs->writeFile(fd, extra.data(), extra.size()) == extra.size());

    data.insert(data.end(), extra.begin(), extra.end());
    expectedContent[name] = std::move(data);

    assert(fs->closeFile(fd));
  }

  // 2. Проверка fileSize
  for (const std::string &name : filenames) {
    size_t size = fs->fileSize(name.c_str());
    assert(size == expectedContent[name].size());
  }

  // 3. Удаление части файлов
  for (const std::string &name : deletedFiles)
    assert(fs->deleteFile(name.c_str()));

  // 4. Проверка findFirst/findNext
  std::set<std::string> expectedRemaining;
  for (const std::string &name : filenames)
    if (!deletedFiles.count(name)) expectedRemaining.insert(name);

  std::set<std::string> found;
  TFile f;
  if (fs->findFirst(f)) {
    found.insert(f.m_FileName);
    while (fs->findNext(f)) {
      found.insert(f.m_FileName);
    }
  }

  assert(expectedRemaining == found);

  // 5. Проверка содержимого каждого оставшегося файла
  for (const std::string &name : expectedRemaining) {
    int fd = fs->openFile(name.c_str(), false);
    assert(fd != -1);
    size_t fSize = fs->fileSize(name.c_str());
    std::vector<uint8_t> buffer(fSize);
    size_t read = fs->readFile(fd, buffer.data(), buffer.size());
    assert(read == fSize);
    assert(memcmp(buffer.data(), expectedContent[name].data(), fSize) == 0);
    assert(fs->closeFile(fd));
  }

  // 6. umount + повторный mount
  assert(fs->umount());
  delete fs;

  fs = CFileSystem::mount(openDisk());
  assert(fs);

  // 7. Повторная проверка файлов и содержимого после монтирования
  for (const std::string &name : expectedRemaining) {
    int fd = fs->openFile(name.c_str(), false);
    assert(fd != -1);
    size_t fSize = fs->fileSize(name.c_str());
    assert(fSize == expectedContent[name].size());

    std::vector<uint8_t> buffer(fSize);
    size_t read = fs->readFile(fd, buffer.data(), fSize);
    assert(read == fSize);
    assert(memcmp(buffer.data(), expectedContent[name].data(), fSize) == 0);
    assert(fs->closeFile(fd));
  }

  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("testUltraComplexFileSystemOperations PASSED\n");
}

static void testSuperStressHeavyComplexScenario() {
  constexpr size_t FILE_COUNT = 10;
  constexpr size_t WRITE_CHUNKS = 4;
  constexpr size_t CHUNK_SIZE_BASE = 256 * 1024; // базовый размер куска 256 KiB
  const std::string prefix = "heavy_";

  std::map<std::string, std::vector<uint8_t>> contentMap;
  std::set<std::string> deletedFiles = { "heavy_3", "heavy_5", "heavy_8" };
  std::string recreatedFile = "heavy_3";
  std::string appendedFile = "heavy_1";

  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  std::mt19937 rng(1337);
  std::uniform_int_distribution<size_t> dist(1, 4);

  // 1. Создание и многократная запись
  for (size_t i = 0; i < FILE_COUNT; ++i) {
    std::string name = prefix + std::to_string(i);
    int fd = fs->openFile(name.c_str(), true);
    assert(fd != -1);

    std::vector<uint8_t> fullData;
    for (size_t w = 0; w < WRITE_CHUNKS; ++w) {
      size_t chunkSize = CHUNK_SIZE_BASE * dist(rng);
      std::vector<uint8_t> part(chunkSize);
      for (size_t j = 0; j < chunkSize; ++j)
        part[j] = static_cast<uint8_t>((j + w + name[0]) % 251);
      assert(fs->writeFile(fd, part.data(), part.size()) == part.size());
      fullData.insert(fullData.end(), part.begin(), part.end());
    }

    contentMap[name] = fullData;
    assert(fs->closeFile(fd));
  }

  // 2. Удаляем некоторые файлы
  for (const std::string &name : deletedFiles)
    assert(fs->deleteFile(name.c_str()));

  // 3. Пересоздаём один удалённый файл
  {
    int fd = fs->openFile(recreatedFile.c_str(), true);
    assert(fd != -1);
    std::vector<uint8_t> newData(777777, 0x77);
    assert(fs->writeFile(fd, newData.data(), newData.size()) == newData.size());
    contentMap[recreatedFile] = newData;
    deletedFiles.erase(recreatedFile); // теперь он существует снова
    assert(fs->closeFile(fd));
  }

  // 4. Дописываем в один из файлов
  {
    int fd = fs->openFile(appendedFile.c_str(), true);
    assert(fd != -1);
    std::vector<uint8_t> extra(128 * 1024, 0x42);
    assert(fs->writeFile(fd, extra.data(), extra.size()) == extra.size());
    contentMap[appendedFile].insert(contentMap[appendedFile].end(), extra.begin(), extra.end());
    assert(fs->closeFile(fd));
  }

  // 5. Проверка через fileSize и чтение
  for (const auto &pair : contentMap) {
    if (deletedFiles.count(pair.first)) continue;
    size_t size = fs->fileSize(pair.first.c_str());
    assert(size == pair.second.size());

    int fd = fs->openFile(pair.first.c_str(), false);
    assert(fd != -1);
    std::vector<uint8_t> buffer(size);
    size_t read = fs->readFile(fd, buffer.data(), size);
    assert(read == size);
    assert(memcmp(buffer.data(), pair.second.data(), size) == 0);
    assert(fs->closeFile(fd));
  }

  // 6. Проверка findFirst/findNext
  std::set<std::string> expectedFiles;
  for (const auto &[name, _] : contentMap)
    if (!deletedFiles.count(name)) expectedFiles.insert(name);

  std::set<std::string> foundFiles;
  TFile f;
  if (fs->findFirst(f)) {
    foundFiles.insert(f.m_FileName);
    while (fs->findNext(f)) {
      foundFiles.insert(f.m_FileName);
    }
  }
  assert(expectedFiles == foundFiles);

  // 7. Нельзя открыть несуществующий файл для чтения
  assert(fs->openFile("nonexistent_file_ABC123", false) == -1);

  // 8. umount/mount + повторная проверка
  assert(fs->umount());
  delete fs;
  fs = CFileSystem::mount(openDisk());
  assert(fs);

  for (const auto &pair : contentMap) {
    if (deletedFiles.count(pair.first)) continue;
    size_t size = fs->fileSize(pair.first.c_str());
    assert(size == pair.second.size());

    int fd = fs->openFile(pair.first.c_str(), false);
    assert(fd != -1);
    std::vector<uint8_t> buffer(size);
    size_t read = fs->readFile(fd, buffer.data(), size);
    assert(read == size);
    assert(memcmp(buffer.data(), pair.second.data(), size) == 0);
    assert(fs->closeFile(fd));
  }

  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("✅ testSuperStressHeavyComplexScenario PASSED\n");
}

void mini() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  int fd = fs->openFile("testfile", true);
  assert(fd != -1);

  std::vector<uint8_t> data(1024 * 1024, 0xAB);
  assert(fs->writeFile(fd, data.data(), data.size()) == data.size());
  assert(fs->closeFile(fd));

  // теперь размер должен быть 1 МБ
  size_t sz = fs->fileSize("testfile");
  assert(sz == data.size());

  assert(fs->umount());
  delete fs;

  // Повторное монтирование
  fs = CFileSystem::mount(dev);
  assert(fs);

  size_t sz2 = fs->fileSize("testfile");
  assert(sz2 == data.size()); // <-- падает тут?

  assert(fs->umount());
  delete fs;
  doneDisk();
}

static void testFileRecreateSameName() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  const char *name = "duplicate_file";

  // 1. Создаём файл и записываем в него
  int fd1 = fs->openFile(name, true);
  assert(fd1 != -1);
  uint8_t data1[1000];
  for (int i = 0; i < 1000; ++i) data1[i] = i % 256;
  assert(fs->writeFile(fd1, data1, 1000) == 1000);
  assert(fs->closeFile(fd1));

  // 2. Удаляем файл
  assert(fs->deleteFile(name));

  // 3. Создаём снова с тем же именем
  int fd2 = fs->openFile(name, true);
  assert(fd2 != -1);
  uint8_t data2[500];
  for (int i = 0; i < 500; ++i) data2[i] = 255 - (i % 256);
  assert(fs->writeFile(fd2, data2, 500) == 500);
  assert(fs->closeFile(fd2));

  // 4. Проверка fileSize()
  size_t size = fs->fileSize(name);
  assert(size == 500);  // ❌ Если получим 0 — ошибка сериализации/дублирования

  // 5. Чтение и проверка содержимого
  int fd3 = fs->openFile(name, false);
  assert(fd3 != -1);
  uint8_t buffer[500];
  size_t read = fs->readFile(fd3, buffer, 500);
  assert(read == 500);
  assert(memcmp(buffer, data2, 500) == 0);
  assert(fs->closeFile(fd3));

  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("✅ testFileRecreateSameName PASSED\n");
}

static void testRecreateAndAppend() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  const char *fname = "same_name";

  // Первое создание
  int fd = fs->openFile(fname, true);
  assert(fd != -1);

  std::vector<uint8_t> data1(3000, 0xAA);
  assert(fs->writeFile(fd, data1.data(), data1.size()) == data1.size());
  assert(fs->closeFile(fd));
  assert(fs->fileSize(fname) == data1.size());

  // Удаление
  assert(fs->deleteFile(fname));
  assert(fs->fileSize(fname) == SIZE_MAX); // Удалили — size должен быть недоступен

  // Повторное создание
  fd = fs->openFile(fname, true);
  assert(fd != -1);
  std::vector<uint8_t> data2(2000, 0xBB);
  assert(fs->writeFile(fd, data2.data(), data2.size()) == data2.size());

  // Дозапись
  std::vector<uint8_t> data3(1000, 0xCC);
  assert(fs->writeFile(fd, data3.data(), data3.size()) == data3.size());
  assert(fs->closeFile(fd));

  // Проверка размеров и содержимого
  size_t expectedSize = data2.size() + data3.size();
  size_t actualSize = fs->fileSize(fname);
  assert(actualSize == expectedSize);

  int readFd = fs->openFile(fname, false);
  assert(readFd != -1);
  std::vector<uint8_t> buffer(expectedSize);
  assert(fs->readFile(readFd, buffer.data(), expectedSize) == expectedSize);

  // Проверка содержимого
  for (size_t i = 0; i < data2.size(); ++i)
    assert(buffer[i] == 0xBB);
  for (size_t i = 0; i < data3.size(); ++i)
    assert(buffer[data2.size() + i] == 0xCC);

  assert(fs->closeFile(readFd));
  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("✅ testRecreateAndAppend PASSED\n");
}

static void testTruncateFileLostMetadata() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  const char *name = "truncate_test";

  // 1. Создаём файл и пишем в него данные
  int fd = fs->openFile(name, true);
  assert(fd != -1);

  std::vector<uint8_t> data(1024, 0x42);
  assert(fs->writeFile(fd, data.data(), data.size()) == data.size());
  assert(fs->closeFile(fd));

  // 2. Открываем файл заново в режиме записи (truncate)
  fd = fs->openFile(name, true);
  assert(fd != -1);
  assert(fs->closeFile(fd)); // ничего не пишем, просто truncate

  // 3. Монтируем повторно и проверяем размер
  assert(fs->umount());
  delete fs;

  fs = CFileSystem::mount(dev);
  assert(fs);

  size_t size = fs->fileSize(name);
  assert(size == 0); // 🔥 если сериализация не была, будет != 0

  assert(fs->umount());
  delete fs;
  doneDisk();

  printf("testTruncateFileLostMetadata PASSED\n");
}

static void testMissingFileSizeAfterRewrite() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  const char *fileName = "missing_file_size";

  // 1. Создать файл и записать в него что-то
  int fd = fs->openFile(fileName, true);
  assert(fd != -1);
  std::vector<uint8_t> data(512 * 1024, 0xAB); // 512 KiB
  assert(fs->writeFile(fd, data.data(), data.size()) == data.size());

  // 2. Закрыть файл
  assert(fs->closeFile(fd));

  // 3. Открыть файл снова для дозаписи
  fd = fs->openFile(fileName, true);
  assert(fd != -1);
  std::vector<uint8_t> extra(128 * 1024, 0xCD); // 128 KiB
  assert(fs->writeFile(fd, extra.data(), extra.size()) == extra.size());

  // ❌ НЕ закрываем файл, сразу umount

  // 4. umount без closeFile
  assert(fs->umount());
  delete fs;

  // 5. Снова монтируем
  fs = CFileSystem::mount(openDisk());
  assert(fs);

  // 🔥 6. Проверка размера
  size_t expectedSize = data.size() + extra.size();
  size_t actualSize = fs->fileSize(fileName);
  assert(actualSize == expectedSize); // 🔥 если нет сериализации — actualSize будет == data.size()

  assert(fs->umount());
  delete fs;
  doneDisk();
  printf("🔥 testMissingFileSizeAfterRewrite PASSED\n");
}
#include <chrono>

void testFileLossOnMount() {
  TBlkDev dev = createDisk();
  assert(CFileSystem::createFs(dev));
  CFileSystem *fs = CFileSystem::mount(dev);
  assert(fs);

  int fd0 = fs->openFile("file_0", true);
  int fd1 = fs->openFile("file_1", true);
  assert(fd0 != -1);
  assert(fd1 != -1);

  // Закрываем файлы (это должно сериализовать их)
  assert(fs->closeFile(fd0));
  assert(fs->closeFile(fd1));

  // Перемонтирование
  assert(fs->umount());
  delete fs;

  fs = CFileSystem::mount(openDisk());
  assert(fs);

  // Проверяем, что оба файла существуют
  int reopened0 = fs->openFile("file_0", false);
  int reopened1 = fs->openFile("file_1", false);
  assert(reopened0 != -1);
  assert(reopened1 != -1);  // 🔥 ВОТ ЗДЕСЬ УПАДЁТ, если файл не сериализовался

  printf("testFileLossOnMount PASSED\n");

  assert(fs->closeFile(reopened0));
  assert(fs->closeFile(reopened1));
  assert(fs->umount());
  delete fs;
  doneDisk();
}

int main () {

  using namespace std::chrono;

  auto start = high_resolution_clock::now();

  testManyFilesVariousSizes();
  testHundredsOfFilesUpTo4KiB();
  testMountPersistence();
  testForcedRelocationAndChunkedRead();
  testArbitraryFilesWithDelete();
  testRelocationWriteMiniCrash();
  testRelocationWriteMinimal();
  testRelocationWrite();
  testFullFilesystemStress();
  testEBEISHIYPROSTOPIZDAYAVAHUE();
  testWriteRead_SingleSector();
  testWriteRead_TwoSectors();
  testWriteRead_4096Chunked();
  testMultiFile4096();
  microTest();
  testMkFs ();
  testWrite ();
  testWriteReadConsistency();
  testSmallFilesContentCheck();
  testTruncateFileLostMetadata();
  testGiantFile();
  testRelocationWriteMinimal_no_truncate();
  testRecreateAndAppend();
  testFileRecreateSameName();
  mini();
  testUltraComplexFileSystemOperations();
  testFindAfterDeletionsWithContentCheck();
  testFindAfterDeletions();
  testOtvalPizdy();
  testComplexFileOperations();
  testEBEISHIYPROSTOPIZDAYAVAHUE();
  testFindNextInconsistency();
  testBigFiles();
  testSmallFilesChunkedRead();
  testDeleting();
  microTest();
  testGiantFile();
  testRelocationWriteMinimal_no_truncate();
  testSimpleAppendAndCheck();

  // FALL
  testFileLossOnMount();
  testMismatchOnRandomSizedFiles();
  testStressLargeFilesComplex();
  ///////////////////////////////////////////

  auto end = high_resolution_clock::now();
  std::cout << "Duration: " << duration_cast<milliseconds>(end - start).count() << " ms\n";
  return EXIT_SUCCESS;
}
