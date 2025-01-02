struct BXFToken {
	enum Type { PARAN, STR, NUM, ID };
	const Type type;
	const string lexeme;

	BXFToken(Type t, string s) : type(t), lexeme(s) {}
};

vector<BXFToken> bxf_tokenize(Reader &reader)
{
	vector<BXFToken> ans;
	int c;
	auto is_num = [](int c) { return '0' <= c && c <= '9'; };
	auto is_letter = [](int c) { return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_'; };
	auto unexpected = [&](int c) {
		if (c == -1)
			cerr << "unexpected eof\n";
		else {
			cerr << reader.file() << ":" << reader.line() << "-" << reader.col() << '\n';
			if (0 < c && c < 128)
				cerr << "unexpected char " << (char)c << '\n';
			else
				cerr << "unexpected char " << c << '\n';
		}
		exit(1);
	};
	while ((c = reader.peek()) != -1) {
		string l;
		if (is_num(c) || c == '-' || c == '+') {
			do
				l += (char)reader.read();
			while (is_num(reader.peek()));
			ans.emplace_back(BXFToken::NUM, l);
		} else if (is_letter(c)) {
			while (is_letter(reader.peek()) || is_num(reader.peek()))
				l += (char)reader.read();
			ans.emplace_back(BXFToken::ID, l);
		} else if (c == '(' || c == ')') {
			l += (char)reader.read();
			ans.emplace_back(BXFToken::PARAN, l);
		} else if (c == '"') {
			reader.read();
			while (reader.peek() != '"' && reader.peek() != -1) {
				l += (char)reader.read();
				if (l.back() == '\\')
					l += (char)reader.read();
			}
			if (reader.peek() != '"')
				unexpected(reader.peek());
			reader.read();
			ans.emplace_back(BXFToken::STR, l);
		} else if (c == '/') {
			reader.read();
			if (reader.peek() == '*') {
				reader.read();
				int state = 0;
				while (state != 2) {
					int x = reader.read();
					if (x == -1)
						unexpected(x);
					if (x == '*')
						state = 1;
					if (state == 1 && x == '/')
						state = 2;
				}
			} else if (reader.peek() == '/') {
				while (reader.read() != '\n') ;
			} else {
				unexpected(c);
			}
		} else if (c < 128 && isspace(c)) {
			reader.read();
		} else {
			unexpected(c);
		}
	}
	return ans;
}

struct BXFNode {
	enum Type { LIST, INT, STR } type;
	string id;
	string str;
	int val;
	vector<BXFNode *> children;

	BXFNode() { type = LIST; }
	BXFNode(const string &s) {
		type = STR;
		str = s;
	}
	BXFNode(int i) {
		type = INT;
		val = i;
	}

	string *type_name() {
		for (auto c : children) {
			if (c->type == LIST && c->id == "text")
				return &c->children[0]->str;
		}
		return 0;
	}

	string *inst_name() {
		bool first = 1;
		for (auto c : children) {
			if (c->type == LIST && c->id == "text") {
				if (first)
					first = 0;
				else
					return &c->children[0]->str;
			}
		}
		return 0;
	}

	BXFNode *first_id(const string &id) {
		for (auto c : children) {
			if (c->type == LIST && c->id == id)
				return c;
		}
		return 0;
	}

	vector<BXFNode *> list_id(const string &id) {
		vector<BXFNode *> ans;
		for (auto c : children) {
			if (c->type == LIST && c->id == id)
				ans.push_back(c);
		}
		return ans;
	}

	BXFNode *clone() const {
		BXFNode *ans = new BXFNode;
		ans->id = id;
		ans->str = str;
		ans->val = val;
		ans->type = type;
		for (auto c : children)
			ans->children.push_back(c->clone());
		return ans;
	}

	static void delete_tree(BXFNode *v) {
		if (v->type != LIST) {
			delete v;
			return;
		}
		for (auto c : v->children)
			delete_tree(c);
		delete v;
	}
};

BXFNode *read_bxf_node(const vector<BXFToken> &vec, size_t &ptr)
{
	auto unexpected = [](const BXFToken &token) {
		cerr << "unexpected token " << token.lexeme << '\n';
		exit(1);
	};

	if (vec[ptr].type == BXFToken::PARAN && vec[ptr].lexeme == "(") {
		ptr++;
		if (vec[ptr].type != BXFToken::ID)
			unexpected(vec[ptr]);
		BXFNode *ans = new BXFNode;
		ans->id = vec[ptr++].lexeme;

		while (vec[ptr].type != BXFToken::PARAN || vec[ptr].lexeme != ")")
			ans->children.push_back(read_bxf_node(vec, ptr));
		ptr++;
		return ans;
	}

	if (vec[ptr].type == BXFToken::STR)
		return new BXFNode(vec[ptr++].lexeme);

	if (vec[ptr].type == BXFToken::NUM)
		return new BXFNode(stoi(vec[ptr++].lexeme));

	unexpected(vec[ptr]);
	return 0;
}

vector<BXFNode *> read_bxf_node_list(const vector<BXFToken> &vec)
{
	size_t ptr = 0;
	vector<BXFNode *> ans;
	while (ptr != vec.size()) {
		ans.push_back(read_bxf_node(vec, ptr));
		if (ans.back()->type != BXFNode::LIST) {
			cerr << "non-list in global scope\n";
			exit(1);
		}
	}
	return ans;
}

struct BXFTableEnt {
	string id;
	vector<string> port;
	vector<string> param;
	BXFNode *node;

	size_t port_search(const string &s) const { return find(port.begin(), port.end(), s) - port.begin(); }
	size_t param_search(const string &s) const { return find(param.begin(), param.end(), s) - param.begin(); }

	BXFTableEnt(BXFNode *node) {
		this->node = node;
		if (node->id == "pin") {
			id = node->children[0]->id;
			return;
		}
		id = *node->type_name();
		for (auto c : node->children) {
			if (c->type == BXFNode::LIST && c->id == "port")
				port.push_back(*c->inst_name());
			if (c->type == BXFNode::LIST && c->id == "parameter")
				param.push_back(c->children[0]->str);
		}
	}

	bool operator<(const BXFTableEnt &ent) const {
		return id < ent.id;
	}
};

map<string, BXFTableEnt *> make_bxf_table(const vector<BXFNode *> &vec)
{
	map<string, BXFTableEnt *> ans;
	for (auto v : vec) {
		if (v->id == "pin" || v->id == "symbol") {
			auto ent = new BXFTableEnt(v);
			ans[ent->id] = ent;
		}
	}
	return ans;
}
