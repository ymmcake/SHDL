struct SHDLToken {
	enum Type { KW, ID, PUNC, STR, NUM, NL };
	Type type;
	string lexeme;

	string file;
	int line, col;
};

vector<SHDLToken> shdl_tokenize(Reader &reader)
{
	vector<SHDLToken> ans;
	int c;
	auto is_num = [](int c) { return '0' <= c && c <= '9'; };
	auto is_letter = [](int c) { return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_'; };
	auto is_punc = [](int c) { return is_in(c, "[]{}()<>,.:;-+/*^"s); };
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
		SHDLToken token;
		token.file = reader.file();
		token.line = reader.line();
		token.col = reader.col();
		auto push = [&](SHDLToken::Type type, const string &lexeme) {
			token.type = type;
			token.lexeme = lexeme;
			ans.push_back(token);
		};
		string l;
		if (is_num(c)) {
			do
				l += (char)reader.read();
			while (is_num(reader.peek()));
			push(SHDLToken::NUM, l);
		} else if (is_letter(c)) {
			while (is_letter(reader.peek()) || is_num(reader.peek()))
				l += (char)reader.read();
			if (is_in(l, vector<string>{"port", "param", "next_col", "input", "output", "bidir"}))
				push(SHDLToken::KW, l);
			else
				push(SHDLToken::ID, l);
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
			push(SHDLToken::STR, l);
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
				while (reader.peek() != '\n')
					reader.read();
			} else {
				push(SHDLToken::PUNC, "/");
			}
		} else if (is_punc(c)) {
			l += (char)reader.read();
			push(SHDLToken::PUNC, l);
		} else if (c == '\n') {
			l += (char)reader.read();
			push(SHDLToken::NL, l);
		} else if (c < 128 && isspace(c)) {
			reader.read();
		} else {
			unexpected(c);
		}
	}
	return ans;
}

struct SHDLEntity {
	string id;
	const BXFTableEnt *tent;
	vector<string> port;
	vector<string> param;
};

vector<SHDLEntity> shdl_read_entities(const vector<SHDLToken> &tokens, const map<string, BXFTableEnt *> table)
{
	vector<SHDLEntity> ans;
	size_t ptr = 0;

	auto unexpected = [](const SHDLToken &tok) {
		cerr << tok.file << ":" << tok.line << "-" << tok.col << " ";
		cerr << "unexpected token " << tok.lexeme << '\n';
		exit(1);
	};
	auto unexpected_eof = []() {
		cerr << "unexpected eof\n";
		exit(1);
	};
	auto undefined = [](string id) {
		cerr << id << " is undefined\n";
		exit(1);
	};
	auto bad_port_param = [](string s) {
		cerr << "bad port or parameter " << s << '\n';
		exit(1);
	};
	auto bad_port_param_unnamed = []() {
		cerr << "unnamed port or param past the end\n";
		exit(1);
	};
	auto port_param_before_entity = []() {
		cerr << "port or param used before entity\n";
		exit(1);
	};
	auto skip_nl = [&]() {
		while (ptr != tokens.size() && tokens[ptr].type == SHDLToken::NL)
			ptr++;
		if (ptr == tokens.size())
			unexpected_eof();
	};
	auto read_till_delim = [&]() {
		skip_nl();
		string ans;
		while (!(tokens[ptr].type == SHDLToken::NL
		         || (tokens[ptr].type == SHDLToken::PUNC && is_in(tokens[ptr].lexeme, vector<string>{";", ":", "}"}))))
			ans += tokens[ptr++].lexeme;
		if (ans.empty())
			unexpected(tokens[ptr]);
		return ans;
	};
	auto read_brace = [&]() {
		skip_nl();
		if (tokens[ptr].type != SHDLToken::PUNC || tokens[ptr].lexeme != "{")
			unexpected(tokens[ptr]);
		ptr++;
		vector<pair<string, string>> ans;
		pair<string, string> one;
		int state = 0;
		skip_nl();
		while (tokens[ptr].type != SHDLToken::PUNC || tokens[ptr].lexeme != "}") {
			string s = read_till_delim();
			if (state == 0) {
				one.first = s;
				state = 1;
			} else if (state == 1) {
				one.second = s;
				state = 2;
			}
			if (tokens[ptr].type != SHDLToken::PUNC || tokens[ptr].lexeme != ":") {
				ans.push_back(one);
				one = {};
				state = 0;
			}
			if (state == 2)
				unexpected(tokens[ptr]);
			if (tokens[ptr].type != SHDLToken::PUNC || tokens[ptr].lexeme != "}")
				ptr++;
			skip_nl();
		}
		ptr++;
		return ans;
	};
	SHDLEntity selected_ent;
	ssize_t port_last, param_last;
	map<string, int> type_cnt;
	while (ptr != tokens.size()) {
		SHDLToken tok = tokens[ptr++];
		if (tok.type == SHDLToken::NL) {
			// nothing
		} else if (tok.type == SHDLToken::KW && is_in(tok.lexeme, vector<string>{"input", "output", "bidir"})) {
			string name = read_till_delim();
			auto it_tent = table.find(tok.lexeme);
			if (it_tent == table.end())
				undefined(tok.lexeme);
			auto tent = it_tent->second;
			ans.push_back({name, tent, {}, {}});
		} else if (tok.type == SHDLToken::ID) {
			if (selected_ent.id.size())
				ans.push_back(selected_ent);
			auto it_tent = table.find(tok.lexeme);
			if (it_tent == table.end())
				undefined(tok.lexeme);
			selected_ent.tent = it_tent->second;
			if (tokens[ptr].type != SHDLToken::ID)
				selected_ent.id = selected_ent.tent->id + "_" + to_string(type_cnt[selected_ent.tent->id]++);
			else
				selected_ent.id = tokens[ptr++].lexeme;
			selected_ent.port.assign(selected_ent.tent->port.size(), "");
			selected_ent.param.assign(selected_ent.tent->param.size(), "");
			param_last = port_last = -1;
		} else if (tok.type == SHDLToken::KW && tok.lexeme == "port") {
			if (selected_ent.id.empty())
				port_param_before_entity();
			auto pairs = read_brace();
			for (auto [l, r] : pairs) {
				if (r.size()) {
					port_last = selected_ent.tent->port_search(l);
					if (port_last == selected_ent.port.size())
						bad_port_param(l);
					selected_ent.port[port_last] = r;
				} else {
					++port_last;
					if (port_last == selected_ent.port.size())
						bad_port_param_unnamed();
					selected_ent.port[port_last] = l;
				}
			}
		} else if (tok.type == SHDLToken::KW && tok.lexeme == "param") {
			if (selected_ent.id.empty())
				port_param_before_entity();
			auto pairs = read_brace();
			for (auto [l, r] : pairs) {
				if (r.size()) {
					param_last = selected_ent.tent->param_search(l);
					if (param_last == selected_ent.param.size())
						bad_port_param(l);
					selected_ent.param[param_last] = r;
				} else {
					++param_last;
					if (param_last == selected_ent.param.size())
						bad_port_param_unnamed();
					selected_ent.param[param_last] = l;
				}
			}
		} else if (tok.type == SHDLToken::KW && tok.lexeme == "next_col") {
			if (selected_ent.id.size())
				ans.push_back(selected_ent);
			selected_ent.id = "";
			ans.push_back({"-next_col"});
		} else {
			unexpected(tok);
		}
	}
	if (selected_ent.id.size())
		ans.push_back(selected_ent);
	return ans;
}
