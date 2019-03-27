/*******************************************************************************
 * cobs/document_list.hpp
 *
 * Copyright (c) 2019 Timo Bingmann
 *
 * All rights reserved. Published under the MIT License in the LICENSE file.
 ******************************************************************************/

#ifndef COBS_DOCUMENT_LIST_HEADER
#define COBS_DOCUMENT_LIST_HEADER

#include <cobs/fasta_file.hpp>
#include <cobs/util/file.hpp>
#include <cobs/util/fs.hpp>

#include <algorithm>
#include <iomanip>

namespace cobs {

enum class FileType {
    Any,
    Text,
    Cortex,
    KMerBuffer,
    Fasta,
    Fastq,
};

/*!
 * DocumentEntry specifies a document or subdocument which can deliver a set of
 * q-grams for indexing.
 */
struct DocumentEntry {
    //! file system path to document
    fs::path path_;
    //! type of document
    FileType type_;
    //! size of the document in bytes
    size_t size_;
    //! subdocument index (for FASTA, FASTQ, etc)
    size_t subdoc_index_ = 0;

    //! default sort operator
    bool operator < (const DocumentEntry& b) const {
        return path_ < b.path_;
    }
};

/*!
 * DocumentList is used to scan a directory, filter the files for specific
 * document types, and then run batch processing methods on them.
 */
class DocumentList
{
public:
    //! accept a file list, sort by name
    DocumentList(const std::vector<DocumentEntry>& list)
        : list_(list) {
        std::sort(list_.begin(), list_.end());
    }

    //! read a directory, filter files
    DocumentList(const fs::path& dir, FileType filter = FileType::Any) {
        fs::recursive_directory_iterator it(dir), end;
        while (it != end) {
            if (accept(*it, filter)) {
                add(*it);
            }
            ++it;
        }
        std::sort(list_.begin(), list_.end());
    }

    //! filter method to match file specifications
    static bool accept(const fs::path& path, FileType filter) {
        switch (filter) {
        case FileType::Any:
            return path.extension() == ".txt" ||
                   path.extension() == ".ctx" ||
                   path.extension() == ".cobs_doc" ||
                   path.extension() == ".fasta" ||
                   path.extension() == ".fastq";
        case FileType::Text:
            return path.extension() == ".txt";
        case FileType::Cortex:
            return path.extension() == ".ctx";
        case FileType::KMerBuffer:
            return path.extension() == ".cobs_doc";
        case FileType::Fasta:
            return path.extension() == ".fasta";
        case FileType::Fastq:
            return path.extension() == ".fastq";
        }
        return false;
    }

    //! add DocumentEntry for given file
    void add(const fs::path& path) {
        if (path.extension() == ".txt") {
            DocumentEntry de;
            de.path_ = path;
            de.type_ = FileType::Text;
            de.size_ = fs::file_size(path);
            list_.emplace_back(de);
        }
        else if (path.extension() == ".ctx") {
            DocumentEntry de;
            de.path_ = path;
            de.type_ = FileType::Cortex;
            de.size_ = fs::file_size(path);
            list_.emplace_back(de);
        }
        else if (path.extension() == ".cobs_doc") {
            DocumentEntry de;
            de.path_ = path;
            de.type_ = FileType::KMerBuffer;
            de.size_ = fs::file_size(path);
            list_.emplace_back(de);
        }
        else if (path.extension() == ".fasta") {
            FastaFile fasta(path);
            for (size_t i = 0; i < fasta.num_documents(); ++i) {
                DocumentEntry de;
                de.path_ = path;
                de.type_ = FileType::Fasta;
                de.size_ = fasta.size(i);
                de.subdoc_index_ = i;
                list_.emplace_back(de);
            }
        }
    }

    //! return list of paths
    const std::vector<DocumentEntry>& list() const { return list_; }

    //! sort files by size
    void sort_by_size() {
        std::sort(list_.begin(), list_.end(),
                  [](const DocumentEntry& d1, const DocumentEntry& d2) {
                      return (std::tie(d1.size_, d1.path_) <
                              std::tie(d2.size_, d2.path_));
                  });
    }

    //! process each file
    void process_each(void (*func)(const DocumentEntry&)) const {
        for (size_t i = 0; i < list_.size(); i++) {
            func(list_[i]);
        }
    }

    //! process files in batch with output file generation
    template <typename Functor>
    void process_batches(size_t batch_size, Functor func) const {
        std::string first_filename, last_filename;
        size_t j = 1;
        std::vector<DocumentEntry> batch;
        for (size_t i = 0; i < list_.size(); i++) {
            std::string filename = cobs::base_name(list_[i].path_);
            if (first_filename.empty()) {
                first_filename = filename;
            }
            last_filename = filename;
            batch.push_back(list_[i]);

            if (batch.size() == batch_size ||
                (!batch.empty() && i + 1 == list_.size()))
            {
                std::string out_file = "[" + first_filename + "-" + last_filename + "]";
                std::cout << "IN"
                          << " - " << std::setfill('0') << std::setw(7) << j
                          << " - " << out_file << std::endl;

                func(batch, out_file);

                std::cout << "OK"
                          << " - " << std::setfill('0') << std::setw(7) << j
                          << " - " << out_file << std::endl;
                batch.clear();
                first_filename.clear();
                j++;
            }
        }
    }

private:
    std::vector<DocumentEntry> list_;
};

} // namespace cobs

#endif // !COBS_DOCUMENT_LIST_HEADER

/******************************************************************************/
