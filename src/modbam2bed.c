// modbam2bed program

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include "htslib/faidx.h"

#include "common.h"
#include "counts.h"
#include "args.h"


int main(int argc, char *argv[]) {
    arguments_t args = parse_arguments(argc, argv);
    fprintf(
        stderr, "Analysing: %s (%s, %c>%c)\n",
        args.mod_base.name, args.mod_base.abbrev, args.mod_base.base, args.mod_base.code);

    // large basecaller runs can produce more files than a single
    // process can open, check this ahead of time.
    struct rlimit reslimit;
    int nfile = 0; for (; args.bam[nfile]; nfile++);
    if (getrlimit(RLIMIT_NOFILE, &reslimit) == 0) {
        if (nfile * args.threads > reslimit.rlim_cur - 100) {
            fprintf(stderr,
                "ERROR: Too many BAM files provided (%i). Try running "
                "samtools merge on subsets of files to produce fewer files", nfile);
            exit(1);
        }
    }

    // load ref sequence
    faidx_t *fai = fai_load(args.ref);
    if (fai == NULL) exit(1);
    if (args.region == NULL) {
        // process all regions
        int nseq = faidx_nseq(fai);
        for (int i = 0; i < nseq; ++i) {
            const char *chr = faidx_iseq(fai, i);
            int len = faidx_seq_len(fai, chr);
            int alen;
            char *ref = faidx_fetch_seq(fai, chr, 0, len, &alen);
            fprintf(stderr, "Fetched %s, %i %i\n", chr, len, alen);
            process_region_threads(args, chr, 0, len, ref);
            free(ref);
        }
    } else {
        // process given region
        int start, end;
        char *chr = xalloc(strlen(args.region) + 1, sizeof(char), "chr");
        strcpy(chr, args.region);
        char *reg_chr = (char *) hts_parse_reg(chr, &start, &end);
        // start and end now zero-based end exclusive
        if (reg_chr) {
            *reg_chr = '\0';  // sets chr to be terminated at correct point
        } else {
            fprintf(stderr, "ERROR: Failed to parse region: '%s'.\n", args.region);
            exit(1);
        }
        // simplify things for later (motif matching) on by fetching whole chr
        int len;
        char *ref = fai_fetch(fai, chr, &len);
        if (len < 0) {
            exit(1);
        }
        end = min(end, len);
        process_region_threads(args, chr, start, end, ref);

        free(chr);
        free(ref);
    }
    fai_destroy(fai);
    exit(0);
}
