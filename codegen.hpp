typedef pair<int,int> pii;

struct Connector {
	pii p1, p2;
	string name;
	bool bus;

	string str() const {
		string s11 = std::to_string(p1.first), s12 = to_string(p1.second), s21 = to_string(p2.first), s22 = to_string(p2.second);
		return "(connector\n\t(text \"" + name + "\" (rect "
			+ s21 + " " + s22 + " " + s21 + " " + s22
			+ ") (font \"Arial\"))\n\t(pt "
			+ s11 + " " + s12
			+ ")\n\t(pt "
			+ s21 + " " + s22
			+ ")\n" + (bus? "\t(bus)\n": "") + ")\n";
	}
};

const string header = "(header \"graphic\" (version \"1.4\"))\n";

int constexpr con_len_h = 48, con_len_v = 16;
int constexpr start_x = 320, start_y = 320;
int constexpr col_len = 400;
int constexpr font_height = 16;
int constexpr spacing = 16;

struct Geo {
	int width, height, ports;
	int x, y;

	int tot_width() { return width; }
	int tot_height() { return height + ((ports>>2)&1) * con_len_v + ((ports>>3)&1) * (con_len_v + font_height); }
	int posx() { return x; }
	int posy() { return y + (ports & 4? con_len_v: 0); }
};

Geo get_geo(BXFNode *v)
{
	int wi = 0, hi = 0, p = 0;
	for (auto u : v->children) {
		if (u->type == BXFNode::LIST && u->id == "rect") {
			wi = u->children[2]->val - u->children[0]->val;
			hi = u->children[3]->val - u->children[1]->val;
			break;
		}
	}
	for (auto u : v->children) {
		if (u->type == BXFNode::LIST && u->id == "port") {
			for (auto w : u->children) {
				if (w->type == BXFNode::LIST && w->id == "pt") {
					if (w->children[0]->val == 0)
						p |= 1;
					else if (w->children[0]->val == wi)
						p |= 2;
					else if (w->children[1]->val == 0)
						p |= 4;
					else if (w->children[1]->val == hi)
						p |= 8;
				}
			}
		}
	}
	return {wi, hi, p};
}

string tabs(int n) {
	string ans;
	while (n--)
		ans += '\t';
	return ans;
}

bool is_bus_name(string s)
{
	for (char c : s)
		if (c == '.' || c == ',')
			return 1;
	return 0;
}

vector<Connector> gen_connectors(vector<BXFNode *> ports, Geo geo, const SHDLEntity &ent)
{
	vector<Connector> ans;
	for (auto port : ports) {
		BXFNode *v = port->first_id("pt");
		pii p0 = {v->children[0]->val, v->children[1]->val};
		pii p1 = {v->children[0]->val + geo.posx(), v->children[1]->val + geo.posy()};
		pii p2 = p1;
		if (p0.first == 0)
			p2 = {p1.first - con_len_h, p1.second};
		else if (p0.first == geo.width)
			p2 = {p1.first + con_len_h, p1.second};
		else if (p0.second == 0)
			p2 = {p1.first, p1.second - con_len_v};
		else if (p0.second == geo.height)
			p2 = {p1.first, p1.second + con_len_v};
		string name = *port->inst_name();
		size_t i = ent.tent->port_search(name);
		if (ent.port[i].size())
			ans.push_back({p1, p2, ent.port[i], is_bus_name(*port->type_name())});
	}
	return ans;
}

void set_params(vector<BXFNode *> params, const SHDLEntity &ent)
{
	for (auto param : params) {
		size_t i = ent.tent->param_search(param->children[0]->str);
		if (ent.param[i].empty())
			continue;
		param->children[1]->str = ent.param[i];
	}
}

void set_pos(BXFNode *rect, int x, int y)
{
	rect->children[2]->val += x - rect->children[0]->val;
	rect->children[3]->val += y - rect->children[1]->val;
	rect->children[0]->val = x;
	rect->children[1]->val = y;
}

void set_pos(BXFNode *rect, Geo geo)
{
	set_pos(rect, geo.posx(), geo.posy());
}

void set_name(BXFNode *node, string name)
{
	*node->inst_name() = name;
}

string bxf_code_gen(BXFNode *v, int depth)
{
	if (v->type == BXFNode::INT)
		return to_string(v->val) + " ";
	if (v->type == BXFNode::STR)
		return "\"" + v->str + "\" ";

	string ans;
	int odepth = depth;
	if (depth != -1)
		ans += tabs(depth);
	ans += "(" + v->id;
	if (depth <= 1 && depth != -1 && v->children.size() && v->children[0]->type == BXFNode::LIST) {
		ans += '\n';
		depth++;
	} else {
		ans += ' ';
		depth = -1;
	}

	for (auto u : v->children) {
		ans += bxf_code_gen(u, depth);
	}

	if (depth != -1)
		ans += tabs(depth-1);
	ans += ")";
	if (odepth != -1)
		ans += "\n";

	return ans;
}

string code_gen(const vector<SHDLEntity> &vec)
{
	string ans = header;
	int x = start_x, y = start_y;
	for (auto &ent : vec) {
		if (ent.id == "-next_col") {
			x += col_len;
			y = start_y;
			continue;
		}

		BXFNode *node = ent.tent->node->clone();

		auto geo = get_geo(node);
		geo.x = x;
		geo.y = y;
		y += geo.tot_height() + spacing;

		set_pos(node->first_id("rect"), geo);

		{
			int ax = geo.x;
			int ay = geo.y + geo.tot_height();
			for (auto p : node->list_id("annotation_block")) {
				auto rect = p->first_id("rect");
				rect->children[3]->val += rect->children[3]->val - rect->children[1]->val - 8;
				set_pos(rect, ax, ay);
				ay += rect->children[3]->val - rect->children[1]->val;
				y += rect->children[3]->val - rect->children[1]->val;
			}
			if (y%8)
				y += 8 - y%8;
		}

		auto cons = gen_connectors(node->list_id("port"), geo, ent);

		set_params(node->list_id("parameter"), ent);

		set_name(node, ent.id);

		ans += bxf_code_gen(node, 0);

		BXFNode::delete_tree(node);

		for (auto &con : cons)
			ans += con.str();
	}
	return ans;
}
