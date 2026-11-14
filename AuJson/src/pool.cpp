#include "aujson.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace aujson {
namespace detail {

namespace {

constexpr size_t kGranule = 16;          // 桶粒度（块 = 8B 头 + 载荷，按 16B 对齐）
constexpr size_t kMaxPoolSize = 1024;    // ≤1KB 走池，更大直接 malloc
constexpr size_t kSlabSize = 64 * 1024;  // slab 大小
constexpr size_t kHeaderSize = 8;        // 块头大小：记录归属，deallocate 据此 O(1) 路由
constexpr uint32_t kMallocIdx = 0xFFFFFFFFu;   // 头标记：malloc 兜底块（回收时整块 free）

constexpr size_t align16(size_t n) { return (n + kGranule - 1) / kGranule * kGranule; }

// 桶数：载荷 ≤1KB → 块 ≤ align16(1024+8)=1040 → 65 桶
constexpr size_t kBucketCount = align16(kMaxPoolSize + kHeaderSize) / kGranule;

std::atomic<bool>& poolEnabledFlag() {
    static std::atomic<bool> f{true};   // 默认开启
    return f;
}

// 块头：紧跟载荷之前。slab 块记录所属 slab 下标；malloc 兜底块记 kMallocIdx。
// deallocate 只会收到本分配器 allocate 返回的指针（STL 契约保证），头可信，
// 无需再线性查找指针归属——这是回收 O(1) 的关键。
struct BlockHeader {
    uint32_t slabIdx;
    uint32_t reserved;
};

// 线程本地池：slab 连续切割 + 按桶内嵌自由链复用（LIFO，缓存友好）。
// 每个 slab 记录 切出数/归还数：全部归还才允许整体释放（slab 内槽位不能单独 free）。
struct Slab {
    char*  start;
    char*  end;
    size_t carved;   // 已切出块数
    size_t freed;    // 已归还块数（复用回退）
};

BlockHeader& headerOf(void* p) {
    return *reinterpret_cast<BlockHeader*>(static_cast<char*>(p) - kHeaderSize);
}

// malloc 兜底：多分配 8B 头并前置，deallocate 端据头识别整块释放
void* mallocChunk(size_t n) {
    char* base = static_cast<char*>(std::malloc(align16(n + kHeaderSize)));
    if (!base) return nullptr;
    *reinterpret_cast<BlockHeader*>(base) = { kMallocIdx, 0 };
    return base + kHeaderSize;
}

// 池必须是 POD（无构造/析构/NSDMI，全零即合法空池）：thread_local 保持平凡，
// 免去 emutls 的初始化守卫与线程退出析构注册（MinGW GCC 7.3 即使 POD 仍走
// emutls——该工具链无原生 TLS，GCC 13+ 的 Windows 目标与 Linux/macOS 的 ELF
// TLS 为原生路径，POD 化后即可直接受益，跨平台零 OS 依赖）。
// 代价：线程退出不自动释放本线程的 slab 缓存（进程退出由系统回收；
// 需要即时回收用 releasePoolCache()），与 tcmalloc/jemalloc 线程缓存策略一致。
struct Pool {
    Slab*  slabs_;                // 手动管理的 slab 数组（std::vector 会破坏 POD 性）
    size_t slabCount_;
    size_t slabCap_;
    void*  free_[kBucketCount];   // 各桶内嵌自由链头（LIFO）
    size_t curIdx_;               // 当前 bump slab 下标（≥ slabCount_ 表示无可用 slab）
    char*  bump_;                 // 当前 bump 位置
};

bool newSlab(Pool& p) {
    if (p.slabCount_ == p.slabCap_) {
        const size_t cap = p.slabCap_ ? p.slabCap_ * 2 : 8;
        Slab* ns = static_cast<Slab*>(std::realloc(p.slabs_, cap * sizeof(Slab)));
        if (!ns) return false;
        p.slabs_ = ns;
        p.slabCap_ = cap;
    }
    char* s = static_cast<char*>(std::malloc(kSlabSize));
    if (!s) return false;
    Slab& slot = p.slabs_[p.slabCount_];
    slot.start = s;
    slot.end = s + kSlabSize;
    slot.carved = 0;
    slot.freed = 0;
    p.curIdx_ = p.slabCount_++;
    p.bump_ = s + kHeaderSize;    // slab 首块的头也写在本 slab 内，起始预留 8B
    return true;
}

void* allocBlock(Pool& p, size_t n) {
    if (!poolEnabledFlag().load(std::memory_order_relaxed))
        return mallocChunk(n);
    const size_t chunk = align16(n + kHeaderSize);
    void*& head = p.free_[chunk / kGranule - 1];
    if (head) {
        void* blk = head;
        head = *static_cast<void**>(blk);          // 内嵌自由链弹出（O(1)）
        BlockHeader& h = headerOf(blk);
        if (h.slabIdx != kMallocIdx)
            p.slabs_[h.slabIdx].freed -= 1;        // 复用：归还计数回退
        return blk;
    }
    if (p.curIdx_ >= p.slabCount_ ||
        static_cast<size_t>(p.slabs_[p.curIdx_].end - p.bump_) < chunk) {
        if (!newSlab(p)) return mallocChunk(n);    // slab 申请失败兜底
    }
    void* blk = p.bump_;
    p.bump_ += chunk;
    p.slabs_[p.curIdx_].carved += 1;
    headerOf(blk) = { static_cast<uint32_t>(p.curIdx_), 0 };
    return blk;
}

void deallocBlock(Pool& p, void* blk, size_t n) noexcept {
    BlockHeader& h = headerOf(blk);
    if (h.slabIdx == kMallocIdx) {
        std::free(&h);                             // 兜底块：头即块基址，整块释放
        return;
    }
    const size_t idx = align16(n + kHeaderSize) / kGranule - 1;
    p.slabs_[h.slabIdx].freed += 1;
    *static_cast<void**>(blk) = p.free_[idx];      // 内嵌自由链压入（O(1)）
    p.free_[idx] = blk;
}

// 有活跃块时保守不释放（slab 内槽位不能单独 free），全部归还才整体回收
void releaseAllBlocks(Pool& p) {
    for (size_t i = 0; i < p.slabCount_; ++i)
        if (p.slabs_[i].freed < p.slabs_[i].carved) return;
    for (size_t i = 0; i < kBucketCount; ++i) p.free_[i] = nullptr;
    for (size_t i = 0; i < p.slabCount_; ++i) std::free(p.slabs_[i].start);
    p.slabCount_ = 0;
    p.curIdx_ = 0;
    p.bump_ = nullptr;
}

Pool& threadPool() {
    static thread_local Pool p;   // 全零即合法空池：常量初始化 → 原生 TLS（免 emutls）
    return p;
}

} // namespace

void* poolAlloc(size_t n) {
    if (n > kMaxPoolSize)
        return std::malloc(n);
    return allocBlock(threadPool(), n);
}

void poolDealloc(void* p, size_t n) noexcept {
    if (n > kMaxPoolSize) {
        std::free(p);
        return;
    }
    deallocBlock(threadPool(), p, n);
}

} // namespace detail

void setSlabPoolEnabled(bool on) {
    detail::poolEnabledFlag().store(on, std::memory_order_relaxed);
}

bool slabPoolEnabled() {
    return detail::poolEnabledFlag().load(std::memory_order_relaxed);
}

void releasePoolCache() {
    detail::releaseAllBlocks(detail::threadPool());
}

} // namespace aujson
