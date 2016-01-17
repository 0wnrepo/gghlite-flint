#include <stdlib.h>
#include <string.h>
#include "matrix.h"
#include "util.h"

bool f2_matrix_copy(f2_matrix *const dest, const f2_matrix src) {
	if(ALLOC_FAILS(dest->elems, src.num_rows))
		return false;
	int *i = &dest->num_rows; /* to shorten some lines */
	int j;
	for(*i = 0; *i < src.num_rows; *i++) {
		if(ALLOC_FAILS(dest->elems[*i], src.num_cols)) {
			f2_matrix_free(*dest);
			return false;
		}

		for(j = 0; j < src.num_cols; j++)
			dest->elems[*i][j] = src.elems[*i][j];
	}
	dest->num_cols = src.num_cols;
}

void f2_matrix_free(f2_matrix m) {
	int i;
	if(NULL != m.elems) {
		for(i = 0; i < m.num_rows; i++)
			free(m.elems[i]);
		free(m.elems);
	}
}

void f2_mbp_free(f2_mbp mbp) {
	int i;
	if(NULL != mbp.matrices) {
		for(i = 0; i < mbp.matrices_len; i++)
			f2_matrix_free(mbp.matrices[i]);
		free(mbp.matrices);
	}
}

void step_free(step s) {
	int i;
	if(NULL != s.symbols) {
		for(i = 0; i < s.symbols_len; i++)
			if(NULL != s.symbols[i])
				free(s.symbols[i]);
		free(s.symbols);
	}
	if(NULL != s.matrix) {
		for(i = 0; i < s.symbols_len; i++)
			f2_matrix_free(s.matrix[i]);
		free(s.matrix);
	}
	if(NULL != s.position) free(s.position);
}

void template_free(template t) {
	int i;
	if(NULL != t.steps) {
		for(i = 0; i < t.steps_len; i++)
			step_free(t.steps[i]);
		free(t.steps);
	}
	if(NULL != t.outputs) {
		for(i = 0; i < t.outputs_len; i++)
			if(NULL != t.outputs[i])
				free(t.outputs[i]);
		free(t.outputs);
	}
}

void plaintext_free(plaintext pt) {
	int i;
	if(NULL != pt.symbols) {
		for(i = 0; i < pt.symbols_len; i++)
			free(pt.symbols[i]);
		free(pt.symbols);
	}
}

bool template_instantiate(const template *const t, const plaintext *const pt, f2_mbp *const mbp) {
	if(t->steps_len != pt->symbols_len) return false;
	if(ALLOC_FAILS(mbp->matrices, t->steps_len))
		return false;

	int *i = &mbp->matrices_len;
	for(*i = 0; *i < t->steps_len; (*i)++) {
		int j;
		bool found = false;
		for(j = 0; j < t->steps[*i].symbols_len; j++) {
			if(!strcmp(pt->symbols[*i], t->steps[*i].symbols[j])) {
				if(found) /* found a second hit! bail out */ {
					f2_matrix_free(mbp->matrices[*i]);
					f2_mbp_free(*mbp);
					return false;
				}
				found = true;

				if(!f2_matrix_copy(mbp->matrices + *i, t->steps[*i].matrix[j])) {
					f2_mbp_free(*mbp);
					return false;
				}
			}
		}
		if(!found) {
			f2_mbp_free(*mbp);
			return false;
		}
	}
	return true;
}