#ifndef ICEPACK_H_
#define ICEPACK_H_

#include <vector>
#include <string>

struct FpgaConfig
{
	std::string device;
	std::string freqrange;
	std::string nosleep;
	std::string warmboot;

	// cram[BANK][X][Y]
	int cram_width, cram_height;
	std::vector<std::vector<std::vector<bool>>> cram;

	// bram[BANK][X][Y]
	int bram_width, bram_height;
	std::vector<std::vector<std::vector<bool>>> bram;

	// data before preamble
	std::vector<uint8_t> initblop;

	// bitstream i/o
	void read_bits(std::istream &ifs);
	void write_bits(std::ostream &ofs) const;

	// icebox i/o
	void read_ascii(std::istream &ifs, bool nosleep);
	void write_ascii(std::ostream &ofs) const;

	// netpbm i/o
	void write_cram_pbm(std::ostream &ofs, int bank_num = -1) const;
	void write_bram_pbm(std::ostream &ofs, int bank_num = -1) const;

	// query chip type metadata
	int chip_width() const;
	int chip_height() const;
	std::vector<int> chip_cols() const;

	// query tile metadata
	std::string tile_type(int x, int y) const;
	int tile_width(const std::string &type) const;

	// cram bit manipulation
	void cram_clear();
	void cram_fill_tiles();
	void cram_checkerboard(int m = 0);
};

#endif /* ICEPACK_H_ */