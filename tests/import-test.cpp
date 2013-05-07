#include <boost/lexical_cast.hpp>
#include <bitcoin/bitcoin.hpp>
using namespace bc;

using std::placeholders::_1;
using std::placeholders::_2;

void blockchain_started(const std::error_code& ec);
void copy_block(const std::error_code& ec, const message::block& blk,
    size_t depth, blockchain* chain_1, blockchain* chain_2);
void handle_import(const std::error_code& ec,
    size_t depth, const hash_digest& hash,
    blockchain* chain_1, blockchain* chain_2);

void blockchain_started(const std::error_code& ec)
{
    if (ec)
        log_error() << "Blockchain init error: " << ec.message();
    else
        log_info() << "Blockchain initialized!";
}

void resume_copy(const std::error_code& ec, size_t last_depth,
    blockchain* chain_1, blockchain* chain_2)
{
    size_t resume_depth = last_depth + 1;
    if (ec == error::not_found)
        resume_depth = 0;
    else if (ec)
    {
        log_error() << "Error fetch last depth from DEST: " << ec.message();
        return;
    }
    fetch_block(*chain_1, resume_depth,
        std::bind(copy_block, _1, _2, resume_depth, chain_1, chain_2));
}

void copy_block(const std::error_code& ec, const message::block& blk,
    size_t depth, blockchain* chain_1, blockchain* chain_2)
{
    if (ec)
        log_error() << "Fetch error: " << ec.message();
    chain_2->import(blk, depth,
        std::bind(&handle_import, _1,
            depth, hash_block_header(blk), chain_1, chain_2));
}

void handle_import(const std::error_code& ec,
    size_t depth, const hash_digest& hash,
    blockchain* chain_1, blockchain* chain_2)
{
    if (ec)
    {
        log_error() << "Import error: " << ec.message();
        return;
    }
    log_info() << "Imported block #" << depth << " " << hash;
    /*if (depth == end_depth)
    {
        log_info() << "Finished.";
        return;
    }*/
    fetch_block(*chain_1, depth + 1,
        std::bind(copy_block, _1, _2, depth + 1, chain_1, chain_2));
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        log_info() << "Usage: import SOURCE DEST";
        return 1;
    }
    async_service service(1);
    bdb_blockchain chain_1(service);
    leveldb_blockchain chain_2(service);
    chain_1.start(argv[1], blockchain_started);
    chain_2.start(argv[2], blockchain_started);
    chain_2.fetch_last_depth(
        std::bind(resume_copy, _1, _2, &chain_1, &chain_2));
    std::cin.get();
    log_info() << "Exiting...";
    service.stop();
    service.join();
    chain_1.stop();
    chain_2.stop();
    return 0;
}
