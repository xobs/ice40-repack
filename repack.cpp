#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <endian.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <iomanip>

#include "icepack.h"

using std::map;
using std::pair;
using std::vector;
using std::string;
using std::ifstream;
using std::getline;

#define error(...) do { fprintf(stderr, "Error: " __VA_ARGS__); exit(1); } while (0)

#define SWAP_BYTES // Define for lm32, undefine for riscv

// static int rom_to_hex(const char *src, const char *dst, uint32_t mem_size) {
//     FILE *outfile;
//     char bfr[4];
//     uint32_t file_size = 0;

//     int fd = open(src, O_RDONLY);
//     if (fd == -1) {
//         fprintf(stderr, "unable to open src rom file %s: %s\n", src, strerror(errno));
//         return 1;
//     }

//     outfile = fopen(dst, "w");
//     if (!outfile) {
//         fprintf(stderr, "unable to open dest rom file %s: %s\n", dst, strerror(errno));
//         return 1;
//     }

//     while (read(fd, bfr, 4) == 4) {
// #ifdef SWAP_BYTES
//         fprintf(outfile, "%02x%02x%02x%02x\n", 0xff & bfr[0], 0xff & bfr[1], 0xff & bfr[2], 0xff & bfr[3]);
// #else
//         fprintf(outfile, "%02x%02x%02x%02x\n", 0xff & bfr[3], 0xff & bfr[2], 0xff & bfr[1], 0xff & bfr[0]);
// #endif
//         file_size += 4;
//     }

//     memset(bfr, 0, sizeof(bfr));
//     while (file_size < mem_size) {
//         fprintf(outfile, "%02x%02x%02x%02x\n", 0xff & bfr[3], 0xff & bfr[2], 0xff & bfr[1], 0xff & bfr[0]);
//         file_size += 4;
//     }
//     fclose(outfile);

//     return 0;
// }

// static int patchup_mem(const char *src, const char *dst, uint32_t *mem_size) {
//     *mem_size = 0;
//     FILE *s = fopen(src, "r");
//     if (!s) {
//         fprintf(stderr, "unable to open source mem file %s: %s\n", src, strerror(errno));
//         return 1;
//     }

//     FILE *d = fopen(dst, "w");
//     if (!d) {
//         fprintf(stderr, "unable to open dest mem file %s: %s\n", dst, strerror(errno));
//         return 2;
//     }

//     char tmpline[128];
//     while (fgets(tmpline, sizeof(tmpline)-1, s) != NULL) {
//         uint32_t word = strtoul(tmpline, NULL, 16);
//         fprintf(d, "%08x\n", word);
//         *mem_size += 4;
//     }

//     fclose(d);
//     fclose(s);
//     return 0;
// }

static void push_back_bitvector(vector<vector<bool>> &hexfile, const vector<int> &digits)
{
	if (digits.empty())
		return;

	hexfile.push_back(vector<bool>(digits.size() * 4));

	for (int i = 0; i < int(digits.size()) * 4; i++)
		if ((digits.at(digits.size() - i/4 -1) & (1 << (i%4))) != 0)
			hexfile.back().at(i) = true;
}

static void parse_hexfile_line(const char *filename, int linenr, vector<vector<bool>> &hexfile, string &line)
{
	vector<int> digits;

	for (char c : line) {
		if ('0' <= c && c <= '9')
			digits.push_back(c - '0');
		else if ('a' <= c && c <= 'f')
			digits.push_back(10 + c - 'a');
		else if ('A' <= c && c <= 'F')
			digits.push_back(10 + c - 'A');
		else if ('x' == c || 'X' == c ||
			 'z' == c || 'Z' == c)
			digits.push_back(0);
		else if ('_' == c)
			;
		else if (' ' == c || '\t' == c || '\r' == c) {
			push_back_bitvector(hexfile, digits);
			digits.clear();
		} else goto error;
	}

	push_back_bitvector(hexfile, digits);

	return;

error:
	fprintf(stderr, "Can't parse line %d of %s: %s\n", linenr, filename, line.c_str());
	exit(1);
}

static void parse_hexfile_32bit_line(const char *filename, int linenr, vector<vector<bool>> &hexfile, string &line, int endian_swap)
{
	vector<int> digits;
    uint32_t word = strtoul(line.data(), NULL, 16);
    if (endian_swap)
        word = (((word >> 24) & 0x000000ff)
             | ((word >> 8) & 0x0000ff00)
             | ((word << 8) & 0x00ff0000)
             | ((word << 24) & 0xff000000));

    std::stringstream stream;
    stream << std::setfill ('0') << std::setw(8) << std::hex << word;
    std::string result(stream.str());

	for (char c : result) {
		if ('0' <= c && c <= '9')
			digits.push_back(c - '0');
		else if ('a' <= c && c <= 'f')
			digits.push_back(10 + c - 'a');
		else if ('A' <= c && c <= 'F')
			digits.push_back(10 + c - 'A');
		else if ('x' == c || 'X' == c ||
			 'z' == c || 'Z' == c)
			digits.push_back(0);
		else if ('_' == c)
			;
		else if (' ' == c || '\t' == c || '\r' == c) {
			push_back_bitvector(hexfile, digits);
			digits.clear();
		} else goto error;
	}

	push_back_bitvector(hexfile, digits);

	return;

error:
	fprintf(stderr, "Can't parse line %d of %s: %s\n", linenr, filename, line.c_str());
	exit(1);
}

static void parse_binfile(const char *filename, std::istream &ifs, vector<vector<bool>> &hexfile)
{
    std::uint32_t n;
    while (ifs.read(reinterpret_cast<char*>(&n), sizeof n)) {
	    vector<int> digits;
        std::stringstream stream;
        stream << std::setfill ('0') << std::setw(8) << std::hex << n;
        std::string result(stream.str());

        // fprintf(stderr, " %08x\n", n);
        // std::cerr << "result: " << result << "\n";
        for (char c : result) {
            if ('0' <= c && c <= '9')
                digits.push_back(c - '0');
            else if ('a' <= c && c <= 'f')
                digits.push_back(10 + c - 'a');
            else if ('A' <= c && c <= 'F')
                digits.push_back(10 + c - 'A');
            else if ('x' == c || 'X' == c ||
                'z' == c || 'Z' == c)
                digits.push_back(0);
            else if ('_' == c)
                ;
            else if (' ' == c || '\t' == c || '\r' == c) {
                push_back_bitvector(hexfile, digits);
                digits.clear();
            } else goto error;
        }

        push_back_bitvector(hexfile, digits);
    }
    return;
error:
	fprintf(stderr, "Can't parse %s\n", filename);
	exit(1);
}

static int icebram_replace(vector<vector<bool>> &from_hexfile,
	                       vector<vector<bool>> &to_hexfile,
                           std::istream &ifs,
                           std::ostream &ofs,
                           bool verbose = false) {
    string line;
    
	if (verbose)
		fprintf(stderr, "Loaded pattern for %d bits wide and %d words deep memory.\n", int(from_hexfile.at(0).size()), int(from_hexfile.size()));

	// -------------------------------------------------------
	// Create bitslices from pattern data

	map<vector<bool>, pair<vector<bool>, int>> pattern;

	for (int i = 0; i < int(from_hexfile.at(0).size()); i++)
	{
		vector<bool> pattern_from, pattern_to;

		for (int j = 0; j < int(from_hexfile.size()); j++)
		{
			pattern_from.push_back(from_hexfile.at(j).at(i));
			pattern_to.push_back(to_hexfile.at(j).at(i));

			if (pattern_from.size() == 256) {
				if (pattern.count(pattern_from)) {
					fprintf(stderr, "Conflicting from pattern for bit slice from_hexfile[%d:%d][%d]!\n", j, j-255, i);
					exit(1);
				}
				pattern[pattern_from] = std::make_pair(pattern_to, 0);
				pattern_from.clear(), pattern_to.clear();
			}
		}

		assert(pattern_from.empty());
		assert(pattern_to.empty());
	}

	if (verbose)
		fprintf(stderr, "Extracted %d bit slices from from/to hexfile data.\n", int(pattern.size()));


	// -------------------------------------------------------
	// Read ascfile from stdin

	vector<string> ascfile_lines;
	map<string, vector<vector<bool>>> ascfile_hexdata;

	for (int i = 1; getline(ifs, line); i++)
	{
	next_asc_stmt:
		ascfile_lines.push_back(line);

		if (line.substr(0, 9) == ".ram_data")
		{
			auto &hexdata = ascfile_hexdata[line];

			for (; getline(ifs, line); i++) {
				if (line.substr(0, 1) == ".")
					goto next_asc_stmt;
                
				parse_hexfile_line("stdin", i, hexdata, line);
			}
		}
	}

	if (verbose)
		fprintf(stderr, "Found %d initialized bram cells in asc file.\n", int(ascfile_hexdata.size()));


	// -------------------------------------------------------
	// Replace bram data

	int max_replace_cnt = 0;

	for (auto &bram_it : ascfile_hexdata)
	{
		auto &bram_data = bram_it.second;

		for (int i = 0; i < 16; i++)
		{
			vector<bool> from_bitslice;

			for (int j = 0; j < 256; j++)
				from_bitslice.push_back(bram_data.at(j / 16).at(16 * (j % 16) + i));

			auto p = pattern.find(from_bitslice);
			if (p != pattern.end())
			{
				auto &to_bitslice = p->second.first;

				for (int j = 0; j < 256; j++)
					bram_data.at(j / 16).at(16 * (j % 16) + i) = to_bitslice.at(j);

				max_replace_cnt = std::max(++p->second.second, max_replace_cnt);
			}
		}
	}

	int min_replace_cnt = max_replace_cnt;
	for (auto &it : pattern)
		min_replace_cnt = std::min(min_replace_cnt, it.second.second);

	if (min_replace_cnt != max_replace_cnt) {
		fprintf(stderr, "Found some bitslices up to %d times, others only %d times!\n", max_replace_cnt, min_replace_cnt);
		exit(1);
	}

	if (verbose)
		fprintf(stderr, "Found and replaced %d instances of the memory.\n", max_replace_cnt);


	// -------------------------------------------------------
	// Write ascfile to stdout

	for (size_t i = 0; i < ascfile_lines.size(); i++) {
		auto &line = ascfile_lines.at(i);
		ofs << line << std::endl;
		if (ascfile_hexdata.count(line)) {
			for (auto &word : ascfile_hexdata.at(line)) {
				for (int k = word.size()-4; k >= 0; k -= 4) {
					int digit = (word[k+3] ? 8 : 0) + (word[k+2] ? 4 : 0) + (word[k+1] ? 2 : 0) + (word[k] ? 1 : 0);
					ofs << "0123456789abcdef"[digit];
				}
				ofs << std::endl;
			}
		}
	}

    return 0;
}

int main(int argc, char **argv) {
    // uint32_t mem_size;
    bool verbose = true;

    if (argc != 5) {
        printf("Usage: %s top.bin mem.init newbios.bin patched-top.bin\n", argv[0]);
        return 1;
    }

	FpgaConfig fpga_config;
	std::stringstream orig_ascii_config;
    std::stringstream new_ascii_config;
    {
        std::ifstream ifs;
        ifs.open(argv[1], std::ios::binary);
        if (!ifs.is_open())
            error("Failed to open input file.\n");

        // Unpack the bitstream into ascii_config
        fpga_config.read_bits(ifs);
        fpga_config.write_ascii(orig_ascii_config);
        orig_ascii_config.seekg(0);
        orig_ascii_config.seekp(0);
    }

    // if (patchup_mem(argv[1], argv[2], &mem_size))
    //     return 1;
    // if (rom_to_hex(argv[3], argv[4], mem_size))
    //     return 1;

    	// -------------------------------------------------------
	// Load from_hexfile and to_hexfile
	vector<vector<bool>> from_hexfile;
	vector<vector<bool>> to_hexfile;
    {
        const char *from_hexfile_n = argv[2];
        ifstream from_hexfile_f(from_hexfile_n);

        const char *to_hexfile_n = argv[3];
        ifstream to_hexfile_f(to_hexfile_n);

        string line;

        parse_binfile(to_hexfile_n, to_hexfile_f, to_hexfile);
       for (int i = 1; getline(from_hexfile_f, line); i++)
           parse_hexfile_32bit_line(from_hexfile_n, i, from_hexfile, line, true);

        if (to_hexfile.size() > 0 && from_hexfile.size() > to_hexfile.size()) {
            if (verbose)
                fprintf(stderr, "Padding to_hexfile from %d words to %d\n",
                    int(to_hexfile.size()), int(from_hexfile.size()));
            do
                to_hexfile.push_back(vector<bool>(to_hexfile.at(0).size()));
            while (from_hexfile.size() > to_hexfile.size());
        }

        if (from_hexfile.size() != to_hexfile.size()) {
            fprintf(stderr, "Hexfiles have different number of words! (%d vs. %d)\n", int(from_hexfile.size()), int(to_hexfile.size()));
            exit(1);
        }

        if (from_hexfile.size() % 256 != 0) {
            fprintf(stderr, "Hexfile number of words (%d) is not divisible by 256!\n", int(from_hexfile.size()));
            exit(1);
        }

        for (size_t i = 1; i < from_hexfile.size(); i++)
            if (from_hexfile.at(i-1).size() != from_hexfile.at(i).size()) {
                fprintf(stderr, "Inconsistent word width at line %d of %s!\n", int(i), from_hexfile_n);
                exit(1);
            }

        for (size_t i = 1; i < to_hexfile.size(); i++) {
            while (to_hexfile.at(i-1).size() > to_hexfile.at(i).size())
                to_hexfile.at(i).push_back(false);
            if (to_hexfile.at(i-1).size() != to_hexfile.at(i).size()) {
                fprintf(stderr, "Inconsistent word width at line %d of %s!\n", int(i+1), to_hexfile_n);
                exit(1);
            }
        }

        if (from_hexfile.size() == 0 || from_hexfile.at(0).size() == 0) {
            fprintf(stderr, "Empty from/to hexfiles!\n");
            exit(1);
        }

   }
    icebram_replace(from_hexfile, to_hexfile, orig_ascii_config, new_ascii_config, verbose);

    // Pack the ascii config into a new bitsream.
    {
        fpga_config.read_ascii(new_ascii_config, false);
        std::ofstream ofs;
        ofs.open(argv[4], std::ios::binary);
        if (!ofs.is_open())
            error("Failed to open input file.\n");
        fpga_config.write_bits(ofs);
    }
    return 0;
}