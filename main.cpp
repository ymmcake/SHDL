#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <utility>
#include <algorithm>
#include <map>
using namespace std;

#include "common.hpp"
#include "bxf.hpp"
#include "shdl.hpp"
#include "codegen.hpp"

int main()
{
	auto libs = read_list("libs.txt");
	auto mylibs = read_list("mylibs.txt");
	auto alllibs = libs;
	alllibs.insert(alllibs.end(), mylibs.begin(), mylibs.end());

	Reader libs_reader(alllibs);
	auto bxf_tokens = bxf_tokenize(libs_reader);
	auto bxf_nodes = read_bxf_node_list(bxf_tokens);
	auto bxf_table = make_bxf_table(bxf_nodes);
	Reader shdl_reader(stdin, "stdin");
	auto shdl_tokens = shdl_tokenize(shdl_reader);
	auto shdl_entities = shdl_read_entities(shdl_tokens, bxf_table);
	cout << code_gen(shdl_entities);
}
