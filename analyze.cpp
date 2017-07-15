#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <zlib.h>

#include "arghandler.h"
#include "District2.h"
#include "Bitmap.h"
#include "Node.h"
#include "uf1.h"
#include "GeoData.h"
#include "placefile.h"
#include "popSSD.h"

#include <vector>
#include <map>
#include <set>

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

void stringVectorAppendCallback(vector<const char*>& context, const char* str) {
    context.push_back(str);
}

class AnalyzeApp {
    public:
    AnalyzeApp();
    int main(int argc, const char** argv);

    int dataExport(const char* exportPath);

    int doCompare(char* fname, char* label);

    void writeText(FILE* textout, const vector<int>& columns, char** labels, const vector<vector<uint32_t> >& counts, int* dsortIndecies, char* fname);
    void writeCSV(FILE* csvout, const vector<int>& columns, char** labels, const vector<vector<uint32_t> >& counts, int* dsortIndecies);
    void writeHtml(FILE* htmlout, const vector<int>& columns, char** labels, const vector<vector<uint32_t> >& counts, int* dsortIndecies);

    int placeSplits();

    protected:
    Solver sov;

    bool distrow;
    bool distcol;
    bool quiet;

    int dsort;

    FILE* textout;
    FILE* csvout;
    FILE* htmlout;

    PlaceNames* placenames;
};

AnalyzeApp::AnalyzeApp()
    : distrow(true), distcol(false), quiet(false),
      dsort(-1),
      textout(NULL),
      csvout(NULL),
      htmlout(NULL)
{}

int AnalyzeApp::dataExport(const char* exportPath) {
    bool exportCsv = false;
    bool gz = false;
    if (strcasestr(exportPath, ".csv")) {
        exportCsv = true;
    }
    if (strcasestr(exportPath, ".gz")) {
        gz = true;
    }
    uint64_t maxUbid = 0;
    for (int i = 0; i < sov.gd->numPoints; ++i) {
        if (sov.gd->ubids[i].ubid > maxUbid) {
            maxUbid = sov.gd->ubids[i].ubid;
        }
    }
    int length = 0;
    while (maxUbid > 0) {
        length++;
        maxUbid /= 10;
    }
    if (length < 4) {
        fprintf(stderr, "unusually short ubid length %d, max ubid=%lu, cowardly exiting\n", length, maxUbid);
        exit(1); return 1;
    } else if (length < 10) {
        length = 10;  // round up to 6 tract + 4 block
    } else if (length < 13) {
        length = 13;  // round up to 3 county + 6 tract + 4 block
    } else if (length < 15) {
        length = 15;  // round up to 2 state + 3 county + 6 tract + 4 block
    }
    char format[50];
    if (exportCsv) {
        snprintf(format, sizeof(format), "%%0%dlld,%%d\n", length);
    } else {
        // fixed length lines
        snprintf(format, sizeof(format), "%%0%dlld%%02d\n", length);
    }
    FILE* exportf = NULL;
    gzFile exportgzf = NULL;
    if (gz) {
        exportgzf = gzopen(exportPath, "wb");
        if (exportgzf == NULL) {
            int gzerrno = 0;
            fprintf(stderr, "gzip error \"%s\" on file \"%s\"\n", gzerror(exportgzf, &gzerrno), exportPath);
            perror(exportPath);
            exit(1);
            return 1;
        }
    } else {
        exportf = fopen(exportPath, "w");
        if (exportf == NULL) {
            perror(exportPath);
            exit(1);
            return 1;
        }
    }
    uint32_t minIndex = 0, maxIndex = 0;
    uint64_t* ubidLut = sov.gd->makeUbidLUT(&minIndex, &maxIndex);
    for (int i = 0; i < sov.gd->numPoints; ++i) {
        uint64_t ubid = ubidLut[i - minIndex];  //sov.gd->ubidOfIndex(i);
        if (gz) {
            gzprintf(exportgzf, format, ubid, sov.winner[i]);
        } else {
            fprintf(exportf, format, ubid, sov.winner[i]);
        }
    }
    delete [] ubidLut;
    if (gz) {
        gzflush(exportgzf, Z_FINISH);
        gzclose(exportgzf);
    } else {
        fclose(exportf);
    }
    return 0;
}

void AnalyzeApp::writeText(FILE* textout, const vector<int>& columns, char** labels, const vector<vector<uint32_t> >& counts, int* dsortIndecies, char* fname) {
    if (distcol) {
        // for each column, print district values
        for (unsigned int col = 0; col < columns.size(); ++col) {
            fprintf(textout, "%s:%s\n", fname, labels[col]);
            for (POPTYPE d = 0; d < sov.districts; ++d) {
                fprintf(textout, "\t%d: %d\n", d, counts[d][col]);
            }
        }
    }

    if (distrow) {
        // for each district, print column values
        for (POPTYPE di = 0; di < sov.districts; ++di) {
            POPTYPE d = dsortIndecies[di];
            fprintf(textout, "distrct:%d\n", d);
            for (unsigned int col = 0; col < columns.size(); ++col) {
                fprintf(textout, "\t%s: %d\n", labels[col], counts[d][col]);
            }
        }
    }
}

void AnalyzeApp::writeCSV(FILE* csvout, const vector<int>& columns, char** labels, const vector<vector<uint32_t> >& counts, int* dsortIndecies) {
    if (distcol) {
        // for each column, print district values
        // header row
        fprintf(csvout, "column");
        for (POPTYPE d = 0; d < sov.districts; ++d) {
            fprintf(csvout, ",%d", d);
        }
        fprintf(csvout, "\n");
        for (unsigned int col = 0; col < columns.size(); ++col) {
            fprintf(csvout, "%s", labels[col]);
            for (POPTYPE d = 0; d < sov.districts; ++d) {
                fprintf(csvout, ",%d", counts[d][col]);
            }
            fprintf(csvout, "\n");
        }
    }

    if (distrow) {
        // for each district, print column values
        // header row
        fprintf(csvout, "district");
        for (unsigned int col = 0; col < columns.size(); ++col) {
            fprintf(csvout, ",%s", labels[col]);
        }
        fprintf(csvout, "\n");
        for (POPTYPE di = 0; di < sov.districts; ++di) {
            POPTYPE d = dsortIndecies[di];
            fprintf(csvout, "%d", d);
            for (unsigned int col = 0; col < columns.size(); ++col) {
                fprintf(csvout, ",%d", counts[d][col]);
            }
            fprintf(csvout, "\n");
        }
    }
}

void AnalyzeApp::writeHtml(FILE* htmlout, const vector<int>& columns, char** labels, const vector<vector<uint32_t> >& counts, int* dsortIndecies) {
    bool printDistrictNumber = false;
    if (distcol) {
        // for each column, print district values
        // header row
        fprintf(htmlout, "<table>");
        if (printDistrictNumber) {
            fprintf(htmlout, "<tr><th>column</th>");
            for (POPTYPE d = 0; d < sov.districts; ++d) {
                fprintf(htmlout, "<th>%d</th>", d);
            }
            fprintf(htmlout, "</tr>\n");
        }
        for (unsigned int col = 0; col < columns.size(); ++col) {
            fprintf(htmlout, "<tr><td>%s</td>", labels[col]);
            for (POPTYPE d = 0; d < sov.districts; ++d) {
                fprintf(htmlout, "<td>%d</td>", counts[d][col]);
            }
            fprintf(htmlout, "</tr>\n");
        }
        fprintf(htmlout, "</table>\n");
    }

    if (distrow) {
        // for each district, print column values
        // header row
        fprintf(htmlout, "<table><tr>");
        if (printDistrictNumber) {
            fprintf(htmlout, "<th>district</th>");
        }
        for (unsigned int col = 0; col < columns.size(); ++col) {
            fprintf(htmlout, "<th>%s</th>", labels[col]);
        }
        fprintf(htmlout, "</tr>\n");
        for (POPTYPE di = 0; di < sov.districts; ++di) {
            POPTYPE d = dsortIndecies[di];
            fprintf(htmlout, "<tr>");
            if (printDistrictNumber) {
                fprintf(htmlout, "<td>%d</td>", d);
            }
            for (unsigned int col = 0; col < columns.size(); ++col) {
                fprintf(htmlout, "<td>%d</td>", counts[d][col]);
            }
            fprintf(htmlout, "</tr>\n");
        }
        fprintf(htmlout, "</table>\n");
    }
}

int AnalyzeApp::main( int argc, const char** argv ) {
	int nargc;

	const char* textOutName = NULL;
	bool notext = false;

	const char* csvOutName = NULL;
	bool nocsv = false;

	const char* htmlOutName = NULL;
	bool nohtml = false;

	const char* exportPath = NULL;
	
	vector<const char*> compareArgs;
	vector<const char*> labelArgs;

	const char* placenamePath = NULL;

	nargc = 1;
	sov.districtSetFactory = District2SetFactory;

	int argi = 1;
	while (argi < argc) {
	    StringArgWithCallback("compare", stringVectorAppendCallback, compareArgs);
	    StringArgWithCallback("labels", stringVectorAppendCallback, labelArgs);
	    IntArg("dsort", &dsort);
	    StringArg("html", &htmlOutName);
	    BoolArg("nohtml", &nohtml);
	    StringArg("text", &textOutName);
	    BoolArg("notext", &notext);
	    StringArg("csv", &csvOutName);
	    BoolArg("nocsv", &nocsv);
	    BoolArg("distrow", &distrow);
	    BoolArg("distcol", &distcol);
	    StringArg("export", &exportPath);

	    StringArg("place-names", &placenamePath);

	    // default:
	    argv[nargc] = argv[argi];
	    nargc++;
	    argi++;
	}
	argv[nargc]=NULL;

	int argcout = sov.handleArgs(nargc, argv);
	if (argcout != 1) {
		// TODO: print local usage here too
		fprintf( stderr, "%s: bogus arg \"%s\"\n", argv[0], argv[1] );
		fputs( Solver::argHelp, stderr );
		exit(1);
		return 1;
	}

        if ((sov.gd->place != NULL) && (placenamePath != NULL)) {
            placenames = PlaceNames::load(placenamePath);
        }

        // Open various output files so they can be appended to while looping through things to compare.

	if (notext) {
	    textout = NULL;
	} else {
	    if ((textOutName == NULL) || (textOutName[0] == '\0') || (0 == strcmp("-", textOutName))) {
		textout = stdout;
		quiet = true;
	    } else {
		textout = fopen(textOutName, "w");
		if (textout == NULL) {
		    perror(textOutName);
		    exit(1);
		}
	    }
	}

	if (nocsv || (csvOutName == NULL)) {
	    csvout = NULL;
	} else {
	    if ((csvOutName[0] == '\0') || (0 == strcmp("-", csvOutName))) {
		csvout = stdout;
		quiet = true;
	    } else {
		csvout = fopen(csvOutName, "w");
		if (csvout == NULL) {
		    perror(csvOutName);
		    exit(1);
		}
	    }
	}

	if (nohtml || (htmlOutName == NULL)) {
	    htmlout = NULL;
	} else {
	    if ((htmlOutName[0] == '\0') || (0 == strcmp("-", htmlOutName))) {
		htmlout = stdout;
		quiet = true;
	    } else {
		htmlout = fopen(htmlOutName, "w");
		if (htmlout == NULL) {
		    perror(htmlOutName);
		    exit(1);
		}
	    }
	}

	sov.load();
	sov.initNodes();
	sov.allocSolution();
	if (sov.hasSolutionToLoad()) {
		if (!quiet) {
		    fprintf(stdout, "loading \"%s\"\n", sov.getSolutionFilename());
		}
		if (sov.hasSolutionToLoad()) {
		    sov.loadSolution();
		}
		if (!quiet) {
			char* statstr = new char[10000];
			sov.getDistrictStats(statstr, 10000);
			fputs(statstr, stdout);
			delete [] statstr;
			double ssd = popSSD(sov.winner, sov.gd, sov.districts);
			fprintf(stdout, "pop FH-ssd: %g\n", ssd);
		}
	}
	
	if (exportPath != NULL) {
            // Take the data we just loaded and write it back out again.
            int ret = dataExport(exportPath);
            if (ret != 0) {
                return ret;
            }
	}

        if (sov.gd->place != NULL) {
            placeSplits();
        }

        // Do sf1 columnar data comparisons
	for (unsigned int i = 0; i < compareArgs.size(); ++i) {
            char* fname = strdup(compareArgs[i]);
            char* label;
            if (labelArgs.size() > i) {
                label = strdup(labelArgs[i]);
            } else {
                // labels are just column numbers being compared
                const char* arg = compareArgs[i];
                label = strdup(strchr(arg, ':') + 1);
            }
            int err = doCompare(fname, label);
            free(fname);
            free(label);
            if (err != 0) {
                return err;
            }
        }
        return 0;
}

typedef std::map<uint64_t, std::set<POPTYPE> > dfptype;

int AnalyzeApp::placeSplits() {
    dfptype districtsForPlaces;
    std::map<uint64_t, uint64_t> placePopulations;
    for (int i = 0; i < sov.gd->numPoints; ++i) {
        uint64_t place = sov.gd->place[i];
        if (place == PlaceMap::INVALID_PLACE || place == 0) {
            continue;
        }
        POPTYPE d = sov.winner[i];
        districtsForPlaces[place].insert(d);
        uint32_t blockIndex = sov.gd->ubids[i].index;
        placePopulations[place] += sov.gd->pop[blockIndex];
    }

    // stats counters
    int placeCount = 0;
    int placesNotSplit = 0;
    int placesSplit = 0;
    vector<uint64_t> splitPlaces;

    for (dfptype::const_iterator it = districtsForPlaces.begin(); it != districtsForPlaces.end(); ++it) {
        placeCount++;
        uint64_t place = it->first;
        const std::set<POPTYPE>& districts = it->second;
        if (districts.size() == 1) {
            placesNotSplit++;
        } else {
            placesSplit++;
            splitPlaces.push_back(place);
        }
    }

    if (htmlout) {
        fprintf(htmlout, "<p>%d places split (%0.3f%%), %d places not split (%0.3f%%)</p>\n", placesSplit, (placesSplit * 1.0) / placeCount, placesNotSplit, (placesNotSplit * 1.0) / placeCount);
        if (placenames != NULL) {
            fprintf(htmlout, "<p id=\"splitplaces\" class=\"hidden\">");
            for (vector<uint64_t>::const_iterator it = splitPlaces.begin(); it != splitPlaces.end(); ++it) {
                uint64_t place = *it;
                const PlaceNames::Place* p = placenames->get(place);
                //fprintf(htmlout, "<span class=\"SPS\">%s %d</span> ", p->name.c_str());
                fprintf(htmlout, "%s\t%lu\n", p->name.c_str(), placePopulations[place]);
            }
            fprintf(htmlout, "</p>\n");
        }
    }
    if (textout) {
        fprintf(textout, "%d places split (%0.3f%%), %d places not split (%0.3f%%)\n", placesSplit, (placesSplit * 1.0) / placeCount, placesNotSplit, (placesNotSplit * 1.0) / placeCount);
    }
    return 0;
}

int AnalyzeApp::doCompare(char* fname, char* label) {
    vector<int> columns;
    parseCompareArg(fname, &columns);
    if (!quiet) {
        fprintf(stdout, "reading \"%s\" columns:", fname);
        for (unsigned int col = 0; col < columns.size(); ++col) {
            fprintf(stdout, " %d", columns[col]);
        }
        fprintf(stdout, "\n");
    }
    vector<uint32_t*> data_columns;
    int recnos_matched;
    bool ok = read_uf1_columns_for_recnos(
                                          sov.gd, fname, columns, &data_columns, &recnos_matched);
    if (!quiet) {
        fprintf(stdout, "%d recnos matched of %d points\n",
                recnos_matched, sov.gd->numPoints);
    }
    if (!ok) {
        fprintf(stderr, "read file \"%s\" failed\n", fname);
        return 1;
    }
    // for each district, for each columns, sum things...
    // counts[district][column]
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
    labels[0] = label;
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
		
    // sort districts based on some column index
    int* dsortIndecies = new int[sov.districts];
    for (int d = 0; d < sov.districts; ++d) {
        dsortIndecies[d] = d;
    }
    if (dsort >= 0 && ((unsigned int)dsort) < columns.size()) {
        bool done = false;
        while (!done) {
            done = true;
            for (int i = 1; i < sov.districts; ++i) {
                if (counts[dsortIndecies[i-1]][dsort] < counts[dsortIndecies[i]][dsort]) {
                    int x = dsortIndecies[i];
                    dsortIndecies[i] = dsortIndecies[i-1];
                    dsortIndecies[i-1] = x;
                    done = false;
                }
            }
        }
    }

    // plain text out
    if (textout != NULL) {
        writeText(textout, columns, labels, counts, dsortIndecies, fname);
    }
		
    // csv out
    if (csvout != NULL) {
        writeCSV(csvout, columns, labels, counts, dsortIndecies);
    }
		
    // html out
    if (htmlout != NULL) {
        writeHtml(htmlout, columns, labels, counts, dsortIndecies);
    }

    for (unsigned int col = 0; col < data_columns.size(); ++col) {
        free(data_columns[col]);
    }

    delete [] labels;
    return 0;
}

int main( int argc, const char** argv ) {
    AnalyzeApp aa;
    return aa.main(argc, argv);
}
