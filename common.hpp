class Reader {
private:
	vector<string> file_list;
	FILE *f;
	int holder, last;
	int col_no, line_no;
	string file_name;

	void open_next() {
		if (file_list.empty()) {
			f = 0;
			return;
		}
		col_no = line_no = 1;
		file_name = file_list.back();
		f = fopen(file_list.back().c_str(), "r");
		if (!f) {
			cerr << "can't open " << file_list.back() << '\n';
			perror("error");
			exit(1);
		}
		file_list.pop_back();
	}

	void unread() {
		if (last == -2) {
			cerr << "internal error: multiple unread\n";
			exit(1);
		}
		holder = last;
		last = -2;
	}

	int read_no_up() {
		if (holder != -2) {
			last = holder;
			holder = -2;
			return last;
		}
		if (!f)
			open_next();
		if (!f)
			return last = -1;
		last = fgetc(f);
		if (last == -1) {
			last = '\n';
			fclose(f);
			f = 0;
		}
		return last;
	}

	void update(int c) {
		col_no++;
		if (c == '\n') {
			col_no = 1;
			line_no++;
		}
	}

	Reader(const Reader &) = delete;

public:
	Reader(const vector<string> &files) {
		file_list = files;
		reverse(file_list.begin(), file_list.end());
		holder = last = -2;
		f = 0;
	}
	Reader(FILE *f, const string &name) {
		holder = last = -2;
		line_no = col_no = 1;
		file_name = name;
		this->f = f;
	}
	~Reader() {
		if (f)
			fclose(f);
	}

	int read() {
		int ans = read_no_up();
		update(ans);
		return ans;
	}

	int peek() {
		int c = read_no_up();
		unread();
		return c;
	}

	int col() { return col_no; }
	int line() { return line_no; }
	string file() { return file_name; }
};

vector<string> read_list(string filename)
{
	ifstream f(filename);
	if (!f.is_open())
		return {};
	vector<string> ans;
	for (;;) {
		string s;
		getline(f, s);
		if (s.size() && s.back() == '\r')
			s.pop_back();
		if (!s.size())
			break;
		ans.push_back(s);
	}
	return ans;
}

template<class Cont, class T>
bool is_in(const T &x, const Cont &cont)
{
	for (auto &y : cont)
		if (x == y)
			return true;
	return false;
}
