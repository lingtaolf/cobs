#pragma once


#include <boost/filesystem/path.hpp>
#include <boost/filesystem.hpp>
#include <file/sample_header.hpp>

namespace genome::msbf {
    void create_folders(const boost::filesystem::path& in_dir, const boost::filesystem::path& out_dir, size_t batch_size) {
        std::vector<boost::filesystem::path> paths;
        boost::filesystem::recursive_directory_iterator it(in_dir), end;
        std::copy_if(it, end, std::back_inserter(paths), [](const auto& p) {
            return boost::filesystem::is_regular_file(p);
        });
        std::sort(paths.begin(), paths.end(), [](const auto& p1, const auto& p2) {
            return boost::filesystem::file_size(p1) < boost::filesystem::file_size(p2);
        });

        size_t batch = 1;
        size_t i = 0;
        boost::filesystem::path sub_out_dir;
        for(const auto& p: paths) {
            if (p.extension() == genome::file::sample_header::file_extension) {
                if (i % batch_size == 0) {
                    sub_out_dir = out_dir.string() + "/" + std::to_string(batch);
                    boost::filesystem::create_directories(sub_out_dir);
                    batch++;
                }
                boost::filesystem::rename(p, sub_out_dir / p.filename());
                i++;
            }
        }
    }

    double calc_bloom_filter_size_ratio(double num_hashes, double false_positive_probability) {
        double denominator = std::log(1 - std::pow(false_positive_probability, 1 / num_hashes));
        double result = -num_hashes / denominator;
        assert(result > 0);
        return result;
    }

    uint64_t calc_bloom_filter_size(size_t num_elements, double num_hashes, double false_positive_probability) {
        double bloom_filter_size_ratio = calc_bloom_filter_size_ratio(num_hashes, false_positive_probability);
        double result = std::ceil(num_elements * bloom_filter_size_ratio);
        assert(result <= UINT64_MAX);
        return result;
    }

    void create_bloom_filters_from_samples(const boost::filesystem::path& in_dir, const boost::filesystem::path& out_dir,
                                           size_t batch_size, size_t num_hashes, double false_positive_probability) {
        assert(batch_size % 8 == 0);
        boost::system::error_code ec;
        for (boost::filesystem::directory_iterator it(in_dir), end; it != end; it.increment(ec)) {
            boost::filesystem::path p = it->path();
            if(boost::filesystem::is_directory(p)) {
                size_t max_file_size = 0;
                for (boost::filesystem::directory_iterator sub_it(p); sub_it != end; sub_it.increment(ec)) {
                    if (sub_it->path().extension() == genome::file::sample_header::file_extension) {
                        max_file_size = std::max(max_file_size, boost::filesystem::file_size(sub_it->path()));
                    }
                }

                size_t bloom_filter_size = calc_bloom_filter_size(max_file_size / 8, num_hashes, false_positive_probability);
                bloom_filter::create_from_samples(p, out_dir / p.filename(), bloom_filter_size, batch_size / 8, num_hashes);
            }
        }
    }

    bool combine_bloom_filters(const boost::filesystem::path& in_dir, const boost::filesystem::path& out_dir, size_t batch_size) {
        bool all_combined = false;
        boost::system::error_code ec;
        for (boost::filesystem::directory_iterator it(in_dir), end; it != end; it.increment(ec)) {
            if (boost::filesystem::is_directory(it->path())) {
                size_t bloom_filter_size = 0;
                size_t num_hashes = 0;
                for (boost::filesystem::directory_iterator bloom_it(it->path());
                     bloom_it != end; bloom_it.increment(ec)) {
                    if (bloom_it->path().extension() == genome::file::bloom_filter_header::file_extension) {
                        std::ifstream ifs;
                        auto bfh = file::deserialize_header<file::bloom_filter_header>(ifs, bloom_it->path());
                        bloom_filter_size = bfh.bloom_filter_size();
                        num_hashes = bfh.num_hashes();
                        break;
                    }
                }
                if (bloom_filter_size != 0) {
                    all_combined = genome::bloom_filter::combine_bloom_filters(in_dir / it->path().filename(), out_dir / it->path().filename(),
                                                                bloom_filter_size, num_hashes,
                                                                batch_size);
                } else {
                    std::cerr << "empty directory" << std::endl;
                }
            }
        }
        return all_combined;
    }

    void create_msbf_from_samples(const boost::filesystem::path& in_dir, const boost::filesystem::path& out_dir,
                                           size_t msbf_batch_size, size_t processing_batch_size, size_t num_hashes, double false_positive_probability) {
        boost::filesystem::path samples_dir = out_dir / "samples/";
        std::string bloom_dir = out_dir.string() +  "/bloom";
        size_t iteration = 1;
        create_folders(in_dir, samples_dir, msbf_batch_size);
        create_bloom_filters_from_samples(samples_dir, bloom_dir + std::to_string(iteration), processing_batch_size, num_hashes, false_positive_probability);
        while(!combine_bloom_filters(bloom_dir + std::to_string(iteration), bloom_dir + std::to_string(iteration + 1), processing_batch_size)) {
            iteration++;
        }
    }
}
