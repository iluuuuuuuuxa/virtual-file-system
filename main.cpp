#ifndef __PROGTEST__
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <functional>

/* Filesystem size: min 8MiB, max 1GiB
 * Filename length: min 1B, max 28B
 * Sector size: 512B
 * Max open files: 8 at a time
 * At most one filesystem mounted at a time.
 * Max file size: < 1GiB
 * Max files in the filesystem: 128
 */

constexpr int FILENAME_LEN_MAX = 28;
constexpr int DIR_ENTRIES_MAX  = 128;
constexpr int OPEN_FILES_MAX   = 8;
constexpr int SECTOR_SIZE      = 512;
constexpr int DEVICE_SIZE_MAX  = ( 1024 * 1024 * 1024 );
constexpr int DEVICE_SIZE_MIN  = ( 8 * 1024 * 1024 );

struct TFile
{
    char    m_FileName[FILENAME_LEN_MAX + 1];
    size_t  m_FileSize;
};

struct TBlkDev
{
    size_t  m_Sectors;
    std::function<size_t(size_t, void *, size_t )> m_Read;
    std::function<size_t(size_t, const void *, size_t )> m_Write;
};
#endif /* __PROGTEST__ */

// FAT sentinel values
static const uint32_t FAT_FREE     = 0xFFFFFFFFU;
static const uint32_t FAT_EOC      = 0xFFFFFFFEU;
static const uint32_t FAT_RESERVED = 0xFFFFFFFDU;

class CFileSystem {
public:
    CFileSystem() = default;

    ~CFileSystem() {
        std::free(m_fat);
        m_fat = nullptr;
    }

    CFileSystem(const CFileSystem &) = delete;
    CFileSystem &operator=(const CFileSystem &) = delete;

    CFileSystem(CFileSystem &&) = delete;
    CFileSystem &operator=(CFileSystem &&) = delete;

    static bool createFs(const TBlkDev &dev);
    static CFileSystem *mount(const TBlkDev &dev);
    bool        umount();
    size_t      fileSize(const char *fileName);
    int         openFile(const char *fileName, bool writeMode);
    bool        closeFile(int fd);
    size_t      readFile(int fd, void *data, size_t len);
    size_t      writeFile(int fd, const void *data, size_t len);
    bool        deleteFile(const char *fileName);
    bool        findFirst(TFile &file);
    bool        findNext(TFile &file);

private:
    TBlkDev    m_dev = {};
    size_t     m_sectors = 0;
    size_t     m_fatStart = 0;
    size_t     m_fatBlocks = 0;
    size_t     m_dirStart = 0;
    size_t     m_dirBlocks = 0;
    size_t     m_dataStart = 0;
    uint32_t  *m_fat = nullptr;

    struct DirEntry {
        bool used;
        char name[FILENAME_LEN_MAX+1];
        size_t size;
        uint32_t firstBlock;
    } m_dir[DIR_ENTRIES_MAX] = {};

    struct OpenFile {
        bool    used;
        bool    writeMode;
        int     dirIndex;
        size_t  pos;
        uint32_t curCluster;
        size_t  curClusterIdx;
    } m_open[OPEN_FILES_MAX] = {};

    size_t m_findCursor = 0;
    uint32_t m_nextFreeCluster = 0;

    uint32_t allocateCluster();
};

bool CFileSystem::createFs(const TBlkDev &dev)
{
    const size_t sectors = dev.m_Sectors;
    const size_t totalBytes = sectors * SECTOR_SIZE;

    if (totalBytes < DEVICE_SIZE_MIN || totalBytes > DEVICE_SIZE_MAX) return false;

    const size_t fatEntries = sectors;
    const size_t fatBytes = fatEntries * sizeof(uint32_t);
    const size_t fatBlocks = (fatBytes + SECTOR_SIZE - 1) / SECTOR_SIZE;
    const size_t dirEntrySize = sizeof(bool)+(FILENAME_LEN_MAX+1)+sizeof(size_t)+sizeof(uint32_t);
    const size_t dirBytes = DIR_ENTRIES_MAX * dirEntrySize;
    const size_t dirBlocks = (dirBytes + SECTOR_SIZE - 1) / SECTOR_SIZE;

    if (sectors <= fatBlocks + dirBlocks) return false;

    auto *fat = (uint32_t*)std::malloc(fatBlocks*SECTOR_SIZE);

    if (!fat) return false;

    const size_t totalEntries = (fatBlocks * SECTOR_SIZE) / sizeof(uint32_t);

    for (size_t i = 0; i < totalEntries; ++i) {
        if (i < fatBlocks + dirBlocks) fat[i] = FAT_RESERVED;
        else if (i < sectors)          fat[i] = FAT_FREE;
        else                            fat[i] = FAT_RESERVED;
    }

    if (dev.m_Write(0, fat, fatBlocks) != fatBlocks) {
        std::free(fat);

        return false;
    }

    std::free(fat);

    void *dirBuf = std::calloc(dirBlocks, SECTOR_SIZE);

    if (!dirBuf) return false;

    bool ok = (dev.m_Write(fatBlocks, dirBuf, dirBlocks) == dirBlocks);
    std::free(dirBuf);

    return ok;
}

CFileSystem *CFileSystem::mount(const TBlkDev &dev) {
    const size_t sectors = dev.m_Sectors;
    const size_t totalBytes = sectors * SECTOR_SIZE;

    if (totalBytes < DEVICE_SIZE_MIN || totalBytes > DEVICE_SIZE_MAX) return nullptr;

    const size_t fatEntries = sectors;
    const size_t fatBytes = fatEntries * sizeof(uint32_t);
    const size_t fatBlocks = (fatBytes + SECTOR_SIZE - 1) / SECTOR_SIZE;
    const size_t dirEntrySize = sizeof(bool)+(FILENAME_LEN_MAX+1)+sizeof(size_t)+sizeof(uint32_t);
    const size_t dirBytes = DIR_ENTRIES_MAX * dirEntrySize;
    const size_t dirBlocks = (dirBytes + SECTOR_SIZE - 1) / SECTOR_SIZE;

    if (sectors <= fatBlocks + dirBlocks) return nullptr;

    auto *fs = new CFileSystem();
    fs->m_dev = dev;
    fs->m_sectors = sectors;
    fs->m_fatStart = 0;
    fs->m_fatBlocks = fatBlocks;
    fs->m_dirStart = fatBlocks;
    fs->m_dirBlocks = dirBlocks;
    fs->m_dataStart = fatBlocks + dirBlocks;
    fs->m_nextFreeCluster = fs->m_dataStart;
    fs->m_fat = (uint32_t*)std::malloc(fatBlocks*SECTOR_SIZE);

    if (!fs->m_fat) {
        delete fs;
        return nullptr;
    }

    if (fs->m_dev.m_Read(fs->m_fatStart, fs->m_fat, fatBlocks) != fatBlocks) {
        std::free(fs->m_fat);
        delete fs;

        return nullptr;
    }

    void *dbuf = std::malloc(dirBlocks * SECTOR_SIZE);

    std::memset(fs->m_open, 0, sizeof(fs->m_open));
    fs->m_findCursor = 0;

    if (fs->m_dev.m_Read(fs->m_dirStart, dbuf, dirBlocks) != dirBlocks) {
        std::free(dbuf);
        std::free(fs->m_fat);
        delete fs;

        return nullptr;
    }

    auto *p = (uint8_t*)dbuf;

    for (size_t i = 0; i < DIR_ENTRIES_MAX; ++i) {
        fs->m_dir[i].used = *p; p += sizeof(bool);
        std::memcpy(fs->m_dir[i].name, p, FILENAME_LEN_MAX+1); p += (FILENAME_LEN_MAX+1);
        std::memcpy(&fs->m_dir[i].size, p, sizeof(size_t)); p += sizeof(size_t);
        std::memcpy(&fs->m_dir[i].firstBlock, p, sizeof(uint32_t)); p += sizeof(uint32_t);
    }

    std::free(dbuf);

    return fs;
}

uint32_t CFileSystem::allocateCluster() {
    for (uint32_t c = m_nextFreeCluster; c < m_sectors; ++c) {
        if (m_fat[c] == FAT_FREE) {
            m_nextFreeCluster = c + 1;

            if (m_nextFreeCluster >= m_sectors)
                m_nextFreeCluster = m_dataStart;

            return c;
        }
    }

    for (uint32_t c = m_dataStart; c < m_nextFreeCluster; ++c) {
        if (m_fat[c] == FAT_FREE) {
            m_nextFreeCluster = c + 1;

            return c;
        }
    }

    return FAT_FREE;
}

bool CFileSystem::umount() {
    bool writeSuccess = (m_dev.m_Write(m_fatStart, m_fat, m_fatBlocks) == m_fatBlocks);

    void *dbuf = std::malloc(m_dirBlocks * SECTOR_SIZE);

    if (dbuf) {
        auto *p = (uint8_t*)dbuf;

        for (size_t i = 0; i < DIR_ENTRIES_MAX; ++i) {
            *p = m_dir[i].used; p += sizeof(bool);
            std::memcpy(p, m_dir[i].name, FILENAME_LEN_MAX+1); p += (FILENAME_LEN_MAX+1);
            std::memcpy(p, &m_dir[i].size, sizeof(size_t)); p += sizeof(size_t);
            std::memcpy(p, &m_dir[i].firstBlock, sizeof(uint32_t)); p += sizeof(uint32_t);
        }

        if (writeSuccess) {
            writeSuccess = (m_dev.m_Write(m_dirStart, dbuf, m_dirBlocks) == m_dirBlocks);
        }

        std::free(dbuf);
    } else {
        writeSuccess = false;
    }

    std::free(m_fat);
    m_fat = nullptr;

    return writeSuccess;
}

size_t CFileSystem::fileSize(const char *fileName)
{
    if(!fileName) return static_cast<size_t>(-1);

    for(size_t i=0;i<DIR_ENTRIES_MAX;++i) {
        if(m_dir[i].used && std::strncmp(m_dir[i].name,fileName,FILENAME_LEN_MAX) == 0)
            return m_dir[i].size;
    }

    return static_cast<size_t>(-1);
}

int CFileSystem::openFile(const char *fileName, bool writeMode) {
    if (!fileName || std::strlen(fileName) > FILENAME_LEN_MAX) return -1;

    // find free fd slot
    int fd = -1;

    for (int i = 0; i < OPEN_FILES_MAX; ++i) {
        if (!m_open[i].used) {
            fd = i;
            break;
        }
    }

    if (fd < 0) return -1;

    // locate or create directory entry
    int dirIdx = -1;

    for (int i = 0; i < DIR_ENTRIES_MAX; ++i) {
        if (m_dir[i].used && std::strncmp(m_dir[i].name, fileName, FILENAME_LEN_MAX) == 0) {
            dirIdx = i;
            break;
        }
    }

    if (writeMode) {
        if (dirIdx >= 0) {
            // truncating existing file: free its cluster chain
            uint32_t cur = m_dir[dirIdx].firstBlock;

            while (cur != FAT_FREE && cur != FAT_EOC) {
                uint32_t nxt = m_fat[cur];
                m_fat[cur] = FAT_FREE;
                cur = nxt;
            }

            m_dir[dirIdx].size = 0;
            m_dir[dirIdx].firstBlock = FAT_FREE;
        } else {
            // creating new file entry
            for (int i = 0; i < DIR_ENTRIES_MAX; ++i) {
                if (!m_dir[i].used) {
                    dirIdx = i;
                    break;
                }
            }

            if (dirIdx < 0) return -1;

            m_dir[dirIdx].used = true;
            std::strncpy(m_dir[dirIdx].name, fileName, FILENAME_LEN_MAX);
            m_dir[dirIdx].name[FILENAME_LEN_MAX] = '\0' ;
            m_dir[dirIdx].size = 0;
            m_dir[dirIdx].firstBlock = FAT_FREE;
        }
    } else {
        // read mode: file must exist
        if (dirIdx < 0) return -1;
    }

    // initialize open file state
    auto &of = m_open[fd];

    of.used = true;
    of.writeMode = writeMode;
    of.dirIndex = dirIdx;
    of.pos = 0;

    if (writeMode) {
        of.curCluster = FAT_FREE;
        of.curClusterIdx = 0;
    } else {
        of.curCluster = m_dir[dirIdx].firstBlock;
        of.curClusterIdx = 0;
    }

    return fd;
}

bool CFileSystem::closeFile(int fd)
{
    if(fd < 0 || fd >= OPEN_FILES_MAX || !m_open[fd].used) return false;

    m_open[fd].used = false;

    return true;
}

size_t CFileSystem::readFile(int fd, void *data, size_t len) {
    if (fd < 0 || fd >= OPEN_FILES_MAX || !m_open[fd].used || m_open[fd].writeMode || len == 0)
        return 0;

    auto &of  = m_open[fd];
    auto &ent = m_dir[of.dirIndex];
    const size_t remain = (of.pos < ent.size) ? ent.size - of.pos : 0;
    const size_t toRead = std::min(len, remain);
    size_t readTotal = 0;
    char buf[SECTOR_SIZE];

    while (readTotal < toRead) {
        size_t offset      = of.pos % SECTOR_SIZE;
        size_t chunk       = std::min(toRead - readTotal, SECTOR_SIZE - offset);
        size_t wantCluster = of.pos / SECTOR_SIZE;
        uint32_t cur       = of.curCluster;
        uint32_t prev      = FAT_EOC;
        size_t idx         = of.curClusterIdx;
        if (wantCluster == idx) {
            // reuse
        } else if (wantCluster == idx + 1) {
            prev = cur;

            cur  = (cur < FAT_EOC ? m_fat[cur] : FAT_FREE);
        } else {
            cur = ent.firstBlock;

            for (size_t i = 0; i < wantCluster && cur < FAT_FREE; ++i) {
                prev = cur;
                cur  = m_fat[cur];
            }
        }

        if (m_dev.m_Read(cur, buf, 1) != 1) break;

        std::memcpy((char*)data + readTotal, buf + offset, chunk);
        readTotal       += chunk;
        of.pos          += chunk;
        of.curCluster    = cur;
        of.curClusterIdx = wantCluster;
    }

    return readTotal;
}

size_t CFileSystem::writeFile(int fd, const void *data, size_t len) {
    if (fd < 0 || fd >= OPEN_FILES_MAX || !m_open[fd].used || !m_open[fd].writeMode || len == 0)
        return 0;

    auto &of  = m_open[fd];
    auto &ent = m_dir[of.dirIndex];
    const char *src = static_cast<const char*>(data);
    size_t written = 0;
    char buf[SECTOR_SIZE];

    while (written < len) {
        size_t offset      = of.pos % SECTOR_SIZE;
        size_t chunk       = std::min(len - written, SECTOR_SIZE - offset);
        size_t wantCluster = of.pos / SECTOR_SIZE;
        uint32_t prev = FAT_EOC;
        uint32_t cur  = of.curCluster;
        size_t idx    = of.curClusterIdx;

        if (wantCluster == idx) {
            // reuse
        } else if (wantCluster == idx + 1) {
            prev = cur;

            cur  = (cur < FAT_EOC ? m_fat[cur] : FAT_FREE);
        } else {
            cur = ent.firstBlock;

            for (size_t i = 0; i < wantCluster && cur < FAT_FREE; ++i) {
                prev = cur;
                cur  = m_fat[cur];
            }
        }

        if (cur == FAT_FREE || cur == FAT_EOC) {
            uint32_t newc = allocateCluster();

            if (newc == FAT_FREE) break;

            m_fat[newc] = FAT_EOC;

            if (prev < FAT_EOC) m_fat[prev] = newc;
            else                ent.firstBlock = newc;

            cur = newc;
        }

        if (offset || chunk < SECTOR_SIZE) {
            m_dev.m_Read(cur, buf, 1);
            std::memcpy(buf + offset, src + written, chunk);
            m_dev.m_Write(cur, buf, 1);
        } else {
            m_dev.m_Write(cur, src + written, 1);
        }

        written          += chunk;
        of.pos            += chunk;
        of.curCluster     = cur;
        of.curClusterIdx  = wantCluster;
    }

    if (of.pos > ent.size) ent.size = of.pos;

    return written;
}

bool CFileSystem::deleteFile(const char * fileName)
{
    if ( !fileName )
        return false;

    for ( size_t i = 0; i < DIR_ENTRIES_MAX; ++i )
    {
        if ( m_dir[i].used &&
             std::strncmp( m_dir[i].name, fileName, FILENAME_LEN_MAX ) == 0 )
        {
            // walk the chain, freeing every cluster up to EOC
            uint32_t cur = m_dir[i].firstBlock;

            while ( cur != FAT_FREE && cur != FAT_EOC )
            {
                uint32_t nxt = m_fat[cur];
                m_fat[cur] = FAT_FREE;
                cur = nxt;
            }

            // clear the directory entry
            m_dir[i].used       = false;
            m_dir[i].firstBlock = FAT_FREE;  // just in case
            m_dir[i].size       = 0;

            return true;
        }
    }

    return false;
}

bool CFileSystem::findFirst(TFile &file)
{
    m_findCursor = 0;

    return findNext(file);
}

bool CFileSystem::findNext(TFile &file)
{
    for(size_t i = m_findCursor; i < DIR_ENTRIES_MAX; ++i) {
        if(m_dir[i].used) {
            std::strncpy(file.m_FileName,m_dir[i].name,FILENAME_LEN_MAX);
            file.m_FileName[FILENAME_LEN_MAX] = '\0';
            file.m_FileSize = m_dir[i].size;
            m_findCursor=i+1;

            return true;
        }
    }

    return false;
}

#ifndef __PROGTEST__
#include "simple_test.inc"
#endif /* __PROGTEST__ */
