#pragma once

#include "server.hpp"


namespace genome {
    class server_mmap : public server {
    private:
        byte* m_data;
        void read_from_disk(const std::vector<size_t>& hashes, char* rows);
    protected:
        void get_counts(const std::vector<size_t>& hashes, std::vector<uint16_t>& counts) override;
    public:
        explicit server_mmap(const boost::filesystem::path& path);
    };
}
