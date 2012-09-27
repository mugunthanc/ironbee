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

/**
 * @file
 * @brief IronAutomata &mdash; Eudoxus DFA Engine Preprocessor Metacode
 *
 * @warning This code is preprocessor metaprogramming code that is effectively
 *          templated on IA_EUDOXUS_ID_T which defines a suitable numeric
 *          type for identifiers and IA_EUDOXUS_PREFIX which defines a
 *          prefix for all types and functions.  This file can not be
 *          directly compiled.  Compile eudoxus.c instead.
 *
 * The IA_EUDOXUS(x) macro prepends IA_EUDOXUS_PREFIX and an underscore to
 * @c x.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef IA_EUDOXUS
#error "IA_EUDOXUS not defined.  Do not compile this file directly."
#endif
#ifndef IA_EUDOXUS_ID_T
#error "IA_EUDOXUS_ID_T not defined.  Do not compile this file directly."
#endif

/**
 * @addtogroup IronAutomataEudoxusAutomata
 *
 * @{
 */

/* Node Specific Implementation */

/* Low degree nodes. */

/**
 * Next function for low degree nodes.
 *
 * @sa IA_EUDOXUS(next) for details.
 */
static
ia_eudoxus_result_t IA_EUDOXUS(next_low)(
    ia_eudoxus_state_t *state
)
{
    if (state == NULL) {
        return IA_EUDOXUS_EINSANE;
    }

    assert(state->eudoxus        != NULL);
    assert(state->callback       != NULL);
    assert(state->node           != NULL);
    assert(state->input_location != NULL);

    const uint8_t c = *(state->input_location);
    bool has_output         = ia_bit8(state->node->header.flags, 0);
    bool has_nonadvancing   = ia_bit8(state->node->header.flags, 1);
    bool has_default        = ia_bit8(state->node->header.flags, 2);
    bool advance_on_default = ia_bit8(state->node->header.flags, 3);
    const IA_EUDOXUS(low_node_t) *node
        = (const IA_EUDOXUS(low_node_t) *)(state->node);

    ia_vls_state_t vls;
    IA_VLS_INIT(vls, node);
    // Advance past first_output.
    IA_VLS_ADVANCE_IF(vls, IA_EUDOXUS_ID_T, has_output);
    IA_EUDOXUS_ID_T default_node = IA_VLS_IF(
        vls,
        IA_EUDOXUS_ID_T,
        0,
        has_default
    );
    uint8_t *advance = IA_VLS_VARRAY_IF(
        vls,
        uint8_t,
        node->out_degree / 8,
        has_nonadvancing
    );
    IA_EUDOXUS(low_edge_t) *edges = IA_VLS_FINAL(
        vls,
        IA_EUDOXUS(low_edge_t)
    );

    IA_EUDOXUS_ID_T next_node            = 0;
    bool            advance_on_next_node = true;

    int             i                    = 0;
    while (i < node->out_degree && edges[i].c != c) {
        ++i;
    }

    if (i == node->out_degree) {
        if (has_default) {
            next_node            = default_node;
            advance_on_next_node = advance_on_default;
        }
    }
    else {
        next_node = edges[i].next_node;
        if (has_nonadvancing) {
            advance_on_default = ia_bitv(advance, i);
        }
    }

    if (next_node == 0) {
        return IA_EUDOXUS_END;
    }

    if (advance_on_next_node) {
        state->input_location  += 1;
        state->remaining_bytes -= 1;
    }

    state->node = (const ia_eudoxus_node_t *)(
        (const char *)(state->eudoxus->automata) + next_node
    );

    return IA_EUDOXUS_OK;
}

/**
 * Output function for low degree nodes.
 *
 * @sa IA_EUDOXUS(output) for details.
 */
static
ia_eudoxus_result_t IA_EUDOXUS(output_low)(
    ia_eudoxus_state_t *state
)
{
    if (state == NULL) {
        return IA_EUDOXUS_EINSANE;
    }

    assert(state->eudoxus  != NULL);
    assert(state->callback != NULL);
    assert(state->node     != NULL);

    bool has_output = ia_bit8(state->node->header.flags, 0);

    if (! has_output) {
        return IA_EUDOXUS_OK;
    }

    const IA_EUDOXUS(low_node_t) *node
        = (const IA_EUDOXUS(low_node_t) *)(state->node);

    ia_vls_state_t vls;
    IA_VLS_INIT(vls, node);
    IA_EUDOXUS_ID_T output = IA_VLS_IF(
        vls,
        IA_EUDOXUS_ID_T,
        0,
        has_output
    );

    if (output == 0) {
        return IA_EUDOXUS_EINVAL;
    }

    while (output != 0) {
        const IA_EUDOXUS(output_t) *output_obj =
            (const IA_EUDOXUS(output_t) *)(
                (const char *)(state->eudoxus->automata) + output
            );
        ia_eudoxus_command_t command = state->callback(
            output_obj->output,
            output_obj->output_length,
            state->input_location,
            state->callback_data
        );
        if (command != IA_EUDOXUS_CMD_CONTINUE) {
            return (ia_eudoxus_result_t)command;
        }
        output = output_obj->next_output;
    }

    return IA_EUDOXUS_OK;
}

/* Node Generic Code */

/**
 * Next function.  Advance state by one step.
 *
 * This function is responsible for advancing the state by one.  This is
 * accomplished by calling the appropriate node specific next function and
 * interpreting interpreting its return code.
 *
 * @param[in,out] state Current state.
 * @return See ia_eudoxus_execute() for return codes meanings.
 */
static
ia_eudoxus_result_t IA_EUDOXUS(next)(
    ia_eudoxus_state_t *state
)
{
    if (state == NULL) {
        return IA_EUDOXUS_EINVAL;
    }

    assert(state->eudoxus != NULL);
    assert(state->node    != NULL);

    ia_eudoxus_result_t result = IA_EUDOXUS_OK;

    switch (state->node->header.type) {
    case 0:
        result = IA_EUDOXUS(next_low)(state);
        break;
    default:
        ia_eudoxus_set_error_printf(
            state->eudoxus,
            "Unknown node type: %d",
            state->node->header.type
        );
        return IA_EUDOXUS_EINVAL;
    }

    switch (result) {
    case IA_EUDOXUS_OK:
    case IA_EUDOXUS_END:
    case IA_EUDOXUS_EINVAL:
    case IA_EUDOXUS_EINSANE:
        /* Nop */
        break;
    case IA_EUDOXUS_EXT_STOP:
    case IA_EUDOXUS_EXT_ERROR:
        ia_eudoxus_set_error_cstr(
            state->eudoxus,
            "Insanity! Nonsense from my next function.  "
            "Please report as bug."
        );
        result = IA_EUDOXUS_EINSANE;
        break;
    default:
        /* Insanity error. */
        ia_eudoxus_set_error_printf(
            state->eudoxus,
            "Insanity! Unknown next result: %d."
            "  Please report as bug.",
            result
        );
        result = IA_EUDOXUS_EINSANE;
    }

    return result;
}

/**
 * Output function.  Run any output callbacks for current state.
 *
 * This function should only be called if a callback is defined, i.e., if
 * @c state->callback is not NULL.
 *
 * As with the previous function, this simply calls an appropriate output
 * function based on node type and interprets the return code.
 *
 * @param[in,out] state Current state.
 * @return See ia_eudoxus_execute() for return codes meanings.
 */
static
ia_eudoxus_result_t IA_EUDOXUS(output)(
    ia_eudoxus_state_t *state
)
{
    if (state == NULL) {
        return IA_EUDOXUS_EINVAL;
    }

    assert(state->eudoxus  != NULL);
    assert(state->node     != NULL);
    assert(state->callback != NULL);

    ia_eudoxus_result_t result = IA_EUDOXUS_OK;

    switch (state->node->header.type) {
    case 0:
        result = IA_EUDOXUS(output_low)(state);
        break;
    default:
        ia_eudoxus_set_error_printf(
            state->eudoxus,
            "Unknown node type: %d",
            state->node->header.type
        );
        return IA_EUDOXUS_EINVAL;
    }

    switch (result) {
    case IA_EUDOXUS_OK:
    case IA_EUDOXUS_STOP:
    case IA_EUDOXUS_ERROR:
    case IA_EUDOXUS_EINVAL:
    case IA_EUDOXUS_EINSANE:
        /* Nop */
        break;
    case IA_EUDOXUS_EXT_NO_NEXT:
        ia_eudoxus_set_error_cstr(
            state->eudoxus,
            "Insanity! Nonsense from my output function.  "
            "Please report as bug."
        );
        return IA_EUDOXUS_EINSANE;
    default:
        /* Insanity error. */
        ia_eudoxus_set_error_printf(
            state->eudoxus,
            "Insanity! Unknown next result: %d."
            "  Please report as bug.",
            result
        );
        return IA_EUDOXUS_EINSANE;
    }

    return result;
}

/**
 * Execute function.  Process a block of input.
 *
 * This is the subengine specific version of ia_eudoxus_execute() and has the
 * same semantics.  It loops through the input, calling the appropriate
 * next and output functions.  If either ever returns a code other than
 * IA_EUDOXUS_OK, it will stop execution and return that code.
 *
 * @param[in, out] state        State of automata.
 * @param[in]      input        Input to execute on.
 * @param[in]      input_length Length of input.
 * @return See ia_eudoxus_execute() for return codes meanings.
 */
static
ia_eudoxus_result_t IA_EUDOXUS(execute)(
    ia_eudoxus_state_t *state,
    const uint8_t      *input,
    size_t              input_length
)
{
    if (state == NULL) {
        return IA_EUDOXUS_EINVAL;
    }

    assert(state->eudoxus != NULL);
    assert(state->node    != NULL);

    ia_eudoxus_set_error(state->eudoxus, NULL);

    if (input == NULL) {
        /* Special case: Rerun output of current node and then resume based
         * on state.
         */
        ia_eudoxus_result_t result = IA_EUDOXUS(output)(state);
        if (result != IA_EUDOXUS_OK) {
            return result;
        }
    }
    else {
        state->input_location  = input;
        state->remaining_bytes = input_length;
    }

    if (state->input_location == NULL) {
        /* Probably state was just created. */
        return IA_EUDOXUS_OK;
    }

    while (state->remaining_bytes > 0) {
        ia_eudoxus_result_t result = IA_EUDOXUS_OK;

        /* Update state, including state->remaining_bytes */
        const uint8_t* old_input_location = state->input_location;
        result = IA_EUDOXUS(next)(state);
        if (result != IA_EUDOXUS_OK) {
            return result;
        }

        /* Call callback. */
        if (
            state->callback != NULL &&
            ( ! state->eudoxus->automata->no_advance_no_output ||
              state->input_location != old_input_location )
        ) {
            result = IA_EUDOXUS(output)(state);
            if (result != IA_EUDOXUS_OK) {
                return result;
            }
        }
    }

    return IA_EUDOXUS_OK;
}

/** @} IronAutomataEudoxusAutomata */