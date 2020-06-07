/* CFFI would issue warning with pragma once */
#ifndef PY_DECISION_TREE_H_INCLUDED
#define PY_DECISION_TREE_H_INCLUDED

#ifndef PY_DECISION_TREE_API
#include "py_decision_tree_export.h"
#define PY_DECISION_TREE_API PY_DECISION_TREE_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct py_decision_tree_context;
typedef struct py_decision_tree_context py_decision_tree_context_t;

PY_DECISION_TREE_API
py_decision_tree_context_t *py_decision_tree_new(unsigned int maxDepth);

PY_DECISION_TREE_API
void py_decision_tree_free(py_decision_tree_context_t *context);

PY_DECISION_TREE_API
void py_decision_tree_deposit(py_decision_tree_context_t *context, const double amount);

PY_DECISION_TREE_API
void py_decision_tree_withdraw(py_decision_tree_context_t *context, const double amount);

PY_DECISION_TREE_API
double py_decision_tree_get_balance(const py_decision_tree_context_t *context);

PY_DECISION_TREE_API
void py_decision_tree_fit(py_decision_tree_context_t *context,
                          const char* datasetPath,
                          const char *attackerFile,
                          const double budget,
                          const unsigned threads);

PY_DECISION_TREE_API
bool py_decision_tree_predict(const py_decision_tree_context_t *context);

PY_DECISION_TREE_API
bool py_decision_tree_is_trained(const py_decision_tree_context_t *context);

PY_DECISION_TREE_API
void py_decision_tree_pretty_print(const py_decision_tree_context_t *context);

#ifdef __cplusplus
}
#endif

#endif /* PY_DECISION_TREE_H_INCLUDED */
