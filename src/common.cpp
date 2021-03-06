#include "common.h"


// Convert optarg type to double
double double_from_optarg(const char *local_optarg) {
    // Check at most the first 8 characters are numerical
    std::string optstring(local_optarg);
    std::string string_double = optstring.substr(0, 8);

    for (std::string::iterator it = string_double.begin(); it != string_double.end(); ++it) {
        if (!isdigit(*it) && (*it) != '.') {
            fprintf(stderr, "This doesn't look like a usable float: %s\n", local_optarg);
            exit(1);
        }
    }

    return std::atof(string_double.c_str());
}


// Convert optarg type to double
int int_from_optarg(const char *optarg) {
    // Check at most the first 8 characters are numerical
    std::string optstring(optarg);
    std::string string_int = optstring.substr(0, 8);

    for (std::string::iterator it = string_int.begin(); it != string_int.end(); ++it) {
        if (!isdigit(*it)) {
            fprintf(stderr, "This doesn't look like a usable integer: %s\n", optarg);
            exit(1);
        }
    }

    return std::atoi(string_int.c_str());
}


// Load an OTU table from file
OtuTable read_otu_table_from_file(std::string &otu_count_fp) {
    //Return variable
    OtuTable otu_table;

    // Variables for storing lines and tokens
    std::string line;
    std::string ele;
    std::stringstream line_stream;
    std::vector<std::vector<double>> otu_counts;
    bool id;

    // Open file stream
    std::ifstream otu_count_fh;
    otu_count_fh.open(otu_count_fp);

    // Process header
    std::getline(otu_count_fh, line);
    line_stream.str(line);

    // Iterate header columns
    while(std::getline(line_stream, ele, '\t')) {
        // Skip the #OTU ID column (first column)
        if (ele == "#OTU ID") {
            continue;
        }

        // Store samples and increment count
        otu_table.sample_names.push_back(ele);
        ++otu_table.sample_number;
    }

    // Process sample counts, need to get OTU names first
    while(std::getline(otu_count_fh, line)) {
        // (Re)sets variables for loop
        id = true;
        line_stream.clear();
        std::vector<double> line_counts;

        // Reverse space for line counts
        line_counts.reserve(otu_table.sample_number);

        // Add current line to line stream and then split by tabs
        line_stream.str(line);
        while (std::getline(line_stream, ele, '\t')) {
            // Grab the #OTU ID
            if (id) {
                otu_table.otu_names.push_back(ele);
                id = false;
                continue;
            }

            // Add current element to OTU count after converting to double; some OTUs may be corrected and therefore a double
            line_counts.push_back(std::stod(ele));
        }

        // Add line_counts to otu_counts nested vector and then increment OTU number
        otu_counts.push_back(line_counts);
        ++otu_table.otu_number;
    }

    // Finally construct the OTU observation matrix directly from the vector memory
    // TODO: check if operator= assigning via copy or is it being optimised out
    otu_table.otu_counts = std::move(otu_counts);

    return otu_table;
}


// Read FASTA sequence from file returning an unordered_map with keys as title and value as sequence
// Ported from Biopython; un-optimised
std::unordered_map<std::string,FastaRecord> read_fasta_from_file(std::string &fasta_fp) {
    // Return variable
    std::unordered_map<std::string,FastaRecord> fasta_records;

    // Variables for storing lines
    std::string line;
    std::stringstream line_stream;

    // Open file stream
    std::ifstream fasta_fh;
    fasta_fh.open(fasta_fp);

    // Skip any text before the first record (e.g. blank lines, comments)
    while (true) {
        std::getline(fasta_fh, line);
        if (line.empty()) {
            // Premature end of file, or just empty?
            fprintf(stderr, "First line of FASTA file was empty, exiting early\n");
            exit(1);
        }
        if (line[0] == '>') {
            break;
        }
    }

    // Begin parsing
    while (true) {
        // TODO: potential error here when FASTA files do not end with a newline
        // Make sure FASTA description starts with > character
        if (line[0] != '>') {
            fprintf(stderr, "Records in Fasta files should start with '>' character\n");
            exit(1);
        }

        // Parse description
        std::string description = line.substr(1, line.size());
        std::string sequence;

        // Collect all FASTA lines in current record
        while (std::getline(fasta_fh, line)) {
            if (line[0] == '>') {
                break;
            }

            // Collecting FASTA
            sequence += line;
        }

        // Create new FastaRecord and add to unordered_map
        FastaRecord fasta_record { description, sequence };
        fasta_records.emplace(description, fasta_record);

        // Check if we're EOF
        if (fasta_fh.eof()) {
            // Finish iteration and return
            return fasta_records;
        }
    }
}


// Sort distances (MergeOtuDistancePairs) by distance, abundance and then name
bool compare_merged_otu_distance_pairs(MergeOtuDistancePair &a, MergeOtuDistancePair &b) {
    // Distance sort
    if (a.distance < b.distance) return true;
    if (b.distance < a.distance) return false;

    // If distance equal, abundance sort
    if (a.merged_otu->abundance > b.merged_otu->abundance) return true;
    if (b.merged_otu->abundance > a.merged_otu->abundance) return false;

    // If distance and abundance is equaly, name sort
    if (a.merged_otu->name < b.merged_otu->name) return true;
    if (b.merged_otu->name < a.merged_otu->name) return false;

    // Should not reach here
    return false;
}


void sort_merged_otu_distance_pair(std::vector<MergeOtuDistancePair> &merged_otu_distance_pairs) {
    std::sort(merged_otu_distance_pairs.begin(), merged_otu_distance_pairs.end(), compare_merged_otu_distance_pairs);
}


// Write the merged OTU counts to file
void write_otu_table_to_file(std::vector<MergeOtu> &merged_otus, OtuData &otu_data, std::string &output_fp) {
    // Get a file handle
    FILE *output_fh = fopen(output_fp.c_str(), "w");

    // Write out header
    fprintf(output_fh, "#OTU ID");
    for (auto &sample_name : otu_data.table->sample_names) {
        fprintf(output_fh, "\t%s", sample_name.c_str());
    }

    // Terminate line with newline character
    fprintf(output_fh, "\n");


    // Iterate rows and counts to write out
    for (auto &merged_otu : merged_otus) {
        // Write sequence as row name
        fprintf(output_fh, "%s", otu_data.table->otu_names[merged_otu.count_index].c_str());

        // Write row counts
        for (auto &count : merged_otu.otu_counts) {
            fprintf(output_fh, "\t%f", count);
        }

        // Terminate line with newline character
        fprintf(output_fh, "\n");
    }
}


void write_merged_otu_members_to_file(std::vector<MergeOtu> &merged_otus, OtuData &otu_data, std::string &output_fp) {
    // Get a file handle
    FILE *output_fh = fopen(output_fp.c_str(), "w");

    // Iterate over merged OTUs and write out members
    for (auto &merged_otu : merged_otus) {
        // Write seed OTU
        fprintf(output_fh, "%s", otu_data.table->otu_names[merged_otu.count_index].c_str());

        // Write members merged into this OTU; skipping the first member as we've processed that one above
        for (std::vector<long long unsigned int>::iterator it = merged_otu.member_count_indices.begin() + 1; it != merged_otu.member_count_indices.end(); ++it) {
            fprintf(output_fh, "\t%s", otu_data.table->otu_names[*it].c_str());
        }

        // Terminate line with newline character
        fprintf(output_fh, "\n");
    }
}
