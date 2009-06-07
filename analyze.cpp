#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "District2.h"
#include "districter.h"
#include "Bitmap.h"
#include "Node.h"
#include "uf1.h"

#include <vector>
using std::vector;

// Command line utility to analyze results for various good-district measures,
// compactness, VRA, etc.

// repalces ':' with '\0', causing arg to become just a file name.
void parseCompareArg(char* arg, vector<int>* columns) {
	while (true) {
		char c;
		c = *arg;
		if (c == '\0') {
			return;
		} else if (c == ':') {
			*arg = '\0';
			arg++;
			break;
		}
		arg++;
	}
	if (*arg == '\0') {
		return;
	}
	char* endp;
	long val;
	while (true) {
		switch (*arg) {
			case '\0': return;
			case ',': arg++; break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				val = strtol(arg, &endp, 10);
				if (endp != arg) {
					columns->push_back(val);
					arg = endp;
					break;
				}
				// fall through
			default:
				fprintf(stderr, "could not parse column \"%s\"\n", arg);
				return;
		}
	}
}

int main( int argc, char** argv ) {
	Solver sov;
	int nargc;
	bool textout = true;
	bool csvout = true;
	bool htmlout = true;
	
	vector<const char*> compareArgs;
	vector<const char*> labelArgs;

	nargc = 1;
	sov.districtSetFactory = District2SetFactory;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--compare")) {
			++i;
			compareArgs.push_back(argv[i]);
		} else if (!strcmp(argv[i], "--labels")) {
			// comma separated list of labels to match a corresponding compare set
			++i;
			labelArgs.push_back(argv[i]);
		} else if (!strcmp(argv[i], "--html")) {
			htmlout = true;
		} else if (!strcmp(argv[i], "--nohtml")) {
			htmlout = false;
		} else if (!strcmp(argv[i], "--text")) {
			textout = true;
		} else if (!strcmp(argv[i], "--notext")) {
			textout = false;
		} else if (!strcmp(argv[i], "--csv")) {
			csvout = true;
		} else if (!strcmp(argv[i], "--nocsv")) {
			csvout = false;
		} else {
			argv[nargc] = argv[i];
			nargc++;
		}
	}
	argv[nargc]=NULL;
	sov.handleArgs(nargc, argv);
	sov.load();
	sov.initNodes();
	sov.allocSolution();
	if (sov.loadname != NULL) {
		fprintf(stdout, "loading \"%s\"\n", sov.loadname);
		if (sov.loadZSolution(sov.loadname) < 0) {
			return 1;
		}
		char* statstr = new char[10000];
		sov.getDistrictStats(statstr, 10000);
		fputs(statstr, stdout);
		delete statstr;
	}
	
	for (unsigned int i = 0; i < compareArgs.size(); ++i) {
		char* fname = strdup(compareArgs[i]);
		vector<int> columns;
		parseCompareArg(fname, &columns);
		fprintf(stdout, "reading \"%s\" columns:", fname);
		for (unsigned int col = 0; col < columns.size(); ++col) {
			fprintf(stdout, " %d", columns[col]);
		}
		fprintf(stdout, "\n");
		vector<uint32_t*> data_columns;
		int recnos_matched;
		bool ok = read_uf1_columns_for_recnos(
			sov.gd, fname, columns, &data_columns, &recnos_matched);
		fprintf(stdout, "%d recnos matched of %d points\n",
			recnos_matched, sov.gd->numPoints);
		if (!ok) {
			fprintf(stderr, "read file \"%s\" failed\n", fname);
			return 1;
		}
		// for each district, for each columns, sum things...
		vector<vector<uint32_t> > counts;
		for (POPTYPE d = 0; d < sov.districts; ++d) {
			counts.push_back(vector<uint32_t>());
			vector<uint32_t>& dc = counts[d];
			for (unsigned int col = 0; col < data_columns.size(); ++col) {
				dc.push_back(0);
			}
		}
		for (int x = 0; x < sov.gd->numPoints; ++x) {
			POPTYPE d = sov.winner[x];
			if (d == NODISTRICT) {
				continue;
			}
			vector<uint32_t>& dc = counts[d];
			for (unsigned int col = 0; col < data_columns.size(); ++col) {
				dc[col] += data_columns[col][x];
			}
		}
		char** labels = new char*[columns.size()];
		if (labelArgs.size() > i) {
			labels[0] = strdup(labelArgs[i]);
		} else {
			const char* arg = compareArgs[i];
			labels[0] = strdup(strchr(arg, ':') + 1);
		}
		{
			int lp = 1;
			int pos = 1;
			while (labels[0][pos] != '\0') {
				if (labels[0][pos] == ',') {
					labels[0][pos] = '\0';
					pos++;
					labels[lp] = labels[0] + pos;
					lp++;
				} else {
					pos++;
				}
			}
		}

		// plain text out
		if (textout) {
			// for each column, print district values
			for (unsigned int col = 0; col < columns.size(); ++col) {
				fprintf(stdout, "%s:%s\n", fname, labels[col]);
				for (POPTYPE d = 0; d < sov.districts; ++d) {
					fprintf(stdout, "\t%d: %d\n", d, counts[d][col]);
				}
			}

			// for each district, print column values
			for (POPTYPE d = 0; d < sov.districts; ++d) {
				fprintf(stdout, "distrct:%d\n", d);
				for (unsigned int col = 0; col < columns.size(); ++col) {
					fprintf(stdout, "\t%s: %d\n", labels[col], counts[d][col]);
				}
			}
		}
		
		// csv out
		if (csvout) {
			// for each column, print district values
			// header row
			fprintf(stdout, "column");
			for (POPTYPE d = 0; d < sov.districts; ++d) {
				fprintf(stdout, ",%d", d);
			}
			fprintf(stdout, "\n");
			for (unsigned int col = 0; col < columns.size(); ++col) {
				fprintf(stdout, "%s", labels[col]);
				for (POPTYPE d = 0; d < sov.districts; ++d) {
					fprintf(stdout, ",%d", counts[d][col]);
				}
				fprintf(stdout, "\n");
			}

			// for each district, print column values
			// header row
			fprintf(stdout, "district");
			for (unsigned int col = 0; col < columns.size(); ++col) {
				fprintf(stdout, ",%s", labels[col]);
			}
			fprintf(stdout, "\n");
			for (POPTYPE d = 0; d < sov.districts; ++d) {
				fprintf(stdout, "%d", d);
				for (unsigned int col = 0; col < columns.size(); ++col) {
					fprintf(stdout, ",%d", counts[d][col]);
				}
				fprintf(stdout, "\n");
			}
		}
		
		// html out
		if (htmlout) {
			// for each column, print district values
			// header row
			fprintf(stdout, "<table><tr><th>column</th>");
			for (POPTYPE d = 0; d < sov.districts; ++d) {
				fprintf(stdout, "<th>%d</th>", d);
			}
			fprintf(stdout, "</tr>\n");
			for (unsigned int col = 0; col < columns.size(); ++col) {
				fprintf(stdout, "<tr><td>%s</td>", labels[col]);
				for (POPTYPE d = 0; d < sov.districts; ++d) {
					fprintf(stdout, "<td>%d</td>", counts[d][col]);
				}
				fprintf(stdout, "</tr>\n");
			}
			fprintf(stdout, "</table>\n");

			// for each district, print column values
			// header row
			fprintf(stdout, "<table><tr><th>district</th>");
			for (unsigned int col = 0; col < columns.size(); ++col) {
				fprintf(stdout, "<th>%s</th>", labels[col]);
			}
			fprintf(stdout, "</tr>\n");
			for (POPTYPE d = 0; d < sov.districts; ++d) {
				fprintf(stdout, "<tr><td>%d</td>", d);
				for (unsigned int col = 0; col < columns.size(); ++col) {
					fprintf(stdout, "<td>%d</td>", counts[d][col]);
				}
				fprintf(stdout, "</tr>\n");
			}
			fprintf(stdout, "</table>\n");
		}

		for (unsigned int col = 0; col < data_columns.size(); ++col) {
			free(data_columns[col]);
		}
		free(fname);
	}
}
