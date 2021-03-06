/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

#ifndef _IB_OPERATOR_H_
#define _IB_OPERATOR_H_

/**
 * @file
 * @brief IronBee --- Operator interface
 *
 * @author Craig Forbes <cforbes@qualys.com>
 */

/**
 * @defgroup IronBeeOperators Operators
 * @ingroup IronBee
 *
 * Operators interpret, modify, or compare data.
 *
 * @{
 */

#include <ironbee/build.h>
#include <ironbee/engine.h>
#include <ironbee/field.h>
#include <ironbee/rule_defs.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Operator Instance Structure */
typedef struct ib_operator_inst_t ib_operator_inst_t;

/**
 * Operator instance creation callback type.
 *
 * When this function is invoked, the @a op_inst operator instance has been
 * created and the op, flags, and params fields have been initialized.
 *
 * @param[in] ib IronBee engine.
 * @param[in] ctx Current context.
 * @param[in] rule The rule that owns the operator instance being executed.
 * @param[in] pool Memory pool to be used for allocating needed memory.
 * @param[in] parameters Unparsed string with the parameters to
 *                       initialize the operator instance.
 * @param[in,out] op_inst Pointer to the operator instance to be initialized.
 *
 * @returns IB_OK if successful.
 */
typedef ib_status_t (* ib_operator_create_fn_t)(
    ib_engine_t        *ib,
    ib_context_t       *ctx,
    const ib_rule_t    *rule,
    ib_mpool_t         *pool,
    const char         *parameters,
    ib_operator_inst_t *op_inst
);

/**
 * Operator instance destruction callback type.
 *
 * This frees any resources allocated to the instance but does not free
 * the instance itself.
 *
 * @param op_inst Operator Instance to be destroyed.
 *
 * @returns IB_OK if successful.
 */
typedef ib_status_t (* ib_operator_destroy_fn_t)(
    ib_operator_inst_t *op_inst
);

/**
 * Operator instance execution callback type.
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] data Instance data needed for execution.
 * @param[in] flags Operator instance flags.
 * @param[in] field The field to operate on.
 * @param[out] result The result of the operator 1=true 0=false.
 *
 * @returns IB_OK if successful.
 */
typedef ib_status_t (* ib_operator_execute_fn_t)(
    const ib_rule_exec_t *rule_exec,
    void                 *data,
    ib_flags_t            flags,
    ib_field_t           *field,
    ib_num_t             *result
);

/** Operator Structure */
typedef struct ib_operator_t ib_operator_t;

struct ib_operator_t {
    char                    *name;       /**< Name of the operator. */
    ib_flags_t               flags;      /**< Operator flags */
    void                    *cd_create;  /**< Create callback data. */
    void                    *cd_destroy; /**< Destroy callback data. */
    void                    *cd_execute; /**< Execute callback data. */
    ib_operator_create_fn_t  fn_create;  /**< Instance creation function. */
    ib_operator_destroy_fn_t fn_destroy; /**< Instance destroy function. */
    ib_operator_execute_fn_t fn_execute; /**< Instance execution function. */
};

/** Operator flags */
#define IB_OP_FLAG_NONE        (0x0)      /**< No flags */
#define IB_OP_FLAG_ALLOW_NULL  (1 << 0)   /**< Op. accepts NULL fields */
#define IB_OP_FLAG_PHASE       (1 << 1)   /**< Op works with phase rules */
#define IB_OP_FLAG_STREAM      (1 << 2)   /**< Op works with stream rules */
#define IB_OP_FLAG_CAPTURE     (1 << 3)   /**< Op supports capture */

struct ib_operator_inst_t {
    struct ib_operator_t *op;      /**< Pointer to the operator type */
    ib_flags_t            flags;   /**< Operator instance flags */
    void                 *data;    /**< Data passed to the execute function */
    char                 *params;  /**< Parameters passed to create */
    ib_field_t           *fparam;  /**< Parameters as a field */
};

/** Operator instance flags */
#define IB_OPINST_FLAG_NONE    (0x0)      /**< No flags */
#define IB_OPINST_FLAG_INVERT  (1 << 0)   /**< Invert the operator */
#define IB_OPINST_FLAG_EXPAND  (1 << 1)   /**< Expand data at runtime */

/**
 * Register an operator.
 *
 * This registers the name and the callbacks used to create, destroy and
 * execute an operator instance.
 *
 * If the name is not unique, an error status will be returned.
 *
 * @param[in] ib Ironbee engine
 * @param[in] name The name of the operator.
 * @param[in] flags Operator flags.
 * @param[in] fn_create A pointer to the instance creation function.
 *                      (May be NULL)
 * @param[in] cd_create Callback data passed to @a fn_create.
 * @param[in] fn_destroy A pointer to the instance destruction function.
 *                       (May be NULL)
 * @param[in] cd_destroy Callback data passed to @a fn_destroy.
 * @param[in] fn_execute A pointer to the operator function.
 *                       If NULL the operator will always return 1 (true).
 * @param[in] cd_execute Callback data passed to @a fn_execute.
 *
 * @returns IB_OK on success, IB_EINVAL if the name is not unique.
 */
ib_status_t DLL_PUBLIC ib_operator_register(
    ib_engine_t              *ib,
    const char               *name,
    ib_flags_t                flags,
    ib_operator_create_fn_t   fn_create,
    void                     *cd_create,
    ib_operator_destroy_fn_t  fn_destroy,
    void                     *cd_destroy,
    ib_operator_execute_fn_t  fn_execute,
    void                     *cd_execute
);

/**
 * Create an operator instance.
 *
 * Looks up the operator by name and executes the operator creation callback.
 *
 * @param[in] ib Ironbee engine
 * @param[in] mpool The memory pool to create the instance from.
 * @param[in] ctx Current IronBee context
 * @param[in] rule The rule that owns the operator instance being executed.
 * @param[in] required_op_flags Required operator flags
 *            (IB_OP_FLAG_{PHASE,STREAM})
 * @param[in] name The name of the operator to create.
 * @param[in] parameters Parameters used to create the instance.
 * @param[in] flags Operator instance flags (i.e. IB_OPINST_FLAG_INVERT)
 * @param[out] op_inst The resulting instance.
 *
 * @returns IB_OK on success,
 *          IB_EINVAL if the required operator flags do not match,
 *          IB_ENOENT if the named operator does not exist
 */
ib_status_t DLL_PUBLIC ib_operator_inst_create_ex(
    ib_engine_t         *ib,
    ib_mpool_t          *mpool,
    ib_context_t        *ctx,
    const ib_rule_t     *rule,
    ib_flags_t           required_op_flags,
    const char          *name,
    const char          *parameters,
    ib_flags_t           flags,
    ib_operator_inst_t **op_inst
);

/**
 * Create an operator instance from the @a ib main memory pool.
 *
 * Looks up the operator by name and executes the operator creation callback.
 *
 * @param[in] ib Ironbee engine
 * @param[in] ctx Current IronBee context
 * @param[in] rule The rule that owns the operator instance being executed.
 * @param[in] required_op_flags Required operator flags
 *            (IB_OP_FLAG_{PHASE,STREAM})
 * @param[in] name The name of the operator to create.
 * @param[in] parameters Parameters used to create the instance.
 * @param[in] flags Operator instance flags (i.e. IB_OPINST_FLAG_INVERT)
 * @param[out] op_inst The resulting instance.
 *
 * @returns IB_OK on success,
 *          IB_EINVAL if the required operator flags do not match,
 *          IB_ENOENT if the named operator does not exist
 */
ib_status_t DLL_PUBLIC ib_operator_inst_create(
    ib_engine_t         *ib,
    ib_context_t        *ctx,
    const ib_rule_t     *rule,
    ib_flags_t           required_op_flags,
    const char          *name,
    const char          *parameters,
    ib_flags_t           flags,
    ib_operator_inst_t **op_inst
);

/**
 * Destroy an operator instance.
 *
 * Destroys any resources held by the operator instance.
 *
 * @param[in] op_inst The instance to destroy
 *
 * @returns IB_OK on success.
 */
ib_status_t DLL_PUBLIC ib_operator_inst_destroy(
    ib_operator_inst_t *op_inst
);

/**
 * Call the execute function for an operator instance.
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] op_inst Operator instance to use.
 * @param[in] field Field to operate on.
 * @param[out] result The result of the operator
 *
 * @returns IB_OK on success
 */
ib_status_t DLL_PUBLIC ib_operator_execute(
    const ib_rule_exec_t     *rule_exec,
    const ib_operator_inst_t *op_inst,
    ib_field_t               *field,
    ib_num_t                 *result
);

#ifdef __cplusplus
}
#endif

/**
 * @} IronBeeOperators
 */

#endif /* _IB_OPERATOR_H_ */
