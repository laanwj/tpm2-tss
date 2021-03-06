/*******************************************************************************
 * Copyright 2017-2018, Fraunhofer SIT sponsored by Infineon Technologies AG
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

#include <sapi/tpm20.h>
#ifndef TSS2_API_VERSION_1_1_1_1
#error Version missmatch among TSS2 header files !
#endif /* TSS2_API_VERSION_1_1_1_1 */
#include "esys_types.h"
#include <esapi/tss2_esys.h>
#include "esys_iutil.h"
#include "esys_mu.h"
#include <sapi/tss2_sys.h>
#define LOGMODULE esys
#include "log/log.h"

/** Store command parameters inside the ESYS_CONTEXT for use during _finish */
static void store_input_parameters (
    ESYS_CONTEXT *esysContext,
    ESYS_TR policySession,
    const TPM2B_TIMEOUT *timeout,
    const TPM2B_DIGEST *cpHashA,
    const TPM2B_NONCE *policyRef,
    const TPM2B_NAME *authName,
    const TPMT_TK_AUTH *ticket)
{
    esysContext->in.PolicyTicket.policySession = policySession;
    if (timeout == NULL) {
        esysContext->in.PolicyTicket.timeout = NULL;
    } else {
        esysContext->in.PolicyTicket.timeoutData = *timeout;
        esysContext->in.PolicyTicket.timeout =
            &esysContext->in.PolicyTicket.timeoutData;
    }
    if (cpHashA == NULL) {
        esysContext->in.PolicyTicket.cpHashA = NULL;
    } else {
        esysContext->in.PolicyTicket.cpHashAData = *cpHashA;
        esysContext->in.PolicyTicket.cpHashA =
            &esysContext->in.PolicyTicket.cpHashAData;
    }
    if (policyRef == NULL) {
        esysContext->in.PolicyTicket.policyRef = NULL;
    } else {
        esysContext->in.PolicyTicket.policyRefData = *policyRef;
        esysContext->in.PolicyTicket.policyRef =
            &esysContext->in.PolicyTicket.policyRefData;
    }
    if (authName == NULL) {
        esysContext->in.PolicyTicket.authName = NULL;
    } else {
        esysContext->in.PolicyTicket.authNameData = *authName;
        esysContext->in.PolicyTicket.authName =
            &esysContext->in.PolicyTicket.authNameData;
    }
    if (ticket == NULL) {
        esysContext->in.PolicyTicket.ticket = NULL;
    } else {
        esysContext->in.PolicyTicket.ticketData = *ticket;
        esysContext->in.PolicyTicket.ticket =
            &esysContext->in.PolicyTicket.ticketData;
    }
}

/** One-Call function for TPM2_PolicyTicket
 *
 * This function invokes the TPM2_PolicyTicket command in a one-call
 * variant. This means the function will block until the TPM response is
 * available. All input parameters are const. The memory for non-simple output
 * parameters is allocated by the function implementation.
 *
 * @param[in,out] esysContext The ESYS_CONTEXT.
 * @param[in] policySession Input handle of type ESYS_TR for
 *     object with handle type TPMI_SH_POLICY.
 * @param[in] shandle1 First session handle.
 * @param[in] shandle2 Second session handle.
 * @param[in] shandle3 Third session handle.
 * @param[in] timeout Input parameter of type TPM2B_TIMEOUT.
 * @param[in] cpHashA Input parameter of type TPM2B_DIGEST.
 * @param[in] policyRef Input parameter of type TPM2B_NONCE.
 * @param[in] authName Input parameter of type TPM2B_NAME.
 * @param[in] ticket Input parameter of type TPMT_TK_AUTH.
 * @retval TSS2_RC_SUCCESS on success
 * @retval TSS2_RC_BAD_SEQUENCE if context is not ready for this function
 * \todo add further error RCs to documentation
 */
TSS2_RC
Esys_PolicyTicket(
    ESYS_CONTEXT *esysContext,
    ESYS_TR policySession,
    ESYS_TR shandle1,
    ESYS_TR shandle2,
    ESYS_TR shandle3,
    const TPM2B_TIMEOUT *timeout,
    const TPM2B_DIGEST *cpHashA,
    const TPM2B_NONCE *policyRef,
    const TPM2B_NAME *authName,
    const TPMT_TK_AUTH *ticket)
{
    TSS2_RC r = TSS2_RC_SUCCESS;

    r = Esys_PolicyTicket_async(esysContext,
                policySession,
                shandle1,
                shandle2,
                shandle3,
                timeout,
                cpHashA,
                policyRef,
                authName,
                ticket);
    return_if_error(r, "Error in async function");

    /* Set the timeout to indefinite for now, since we want _finish to block */
    int32_t timeouttmp = esysContext->timeout;
    esysContext->timeout = -1;
    /*
     * Now we call the finish function, until return code is not equal to
     * from TSS2_BASE_RC_TRY_AGAIN.
     * Note that the finish function may return TSS2_RC_TRY_AGAIN, even if we
     * have set the timeout to -1. This occurs for example if the TPM requests
     * a retransmission of the command via TPM2_RC_YIELDED.
     */
    do {
        r = Esys_PolicyTicket_finish(esysContext);
        /* This is just debug information about the reattempt to finish the
           command */
        if ((r & ~TSS2_RC_LAYER_MASK) == TSS2_BASE_RC_TRY_AGAIN)
            LOG_DEBUG("A layer below returned TRY_AGAIN: %" PRIx32
                      " => resubmitting command", r);
    } while ((r & ~TSS2_RC_LAYER_MASK) == TSS2_BASE_RC_TRY_AGAIN);

    /* Restore the timeout value to the original value */
    esysContext->timeout = timeouttmp;
    return_if_error(r, "Esys Finish");

    return TSS2_RC_SUCCESS;
}

/** Asynchronous function for TPM2_PolicyTicket
 *
 * This function invokes the TPM2_PolicyTicket command in a asynchronous
 * variant. This means the function will return as soon as the command has been
 * sent downwards the stack to the TPM. All input parameters are const.
 * In order to retrieve the TPM's response call Esys_PolicyTicket_finish.
 *
 * @param[in,out] esysContext The ESYS_CONTEXT.
 * @param[in] policySession Input handle of type ESYS_TR for
 *     object with handle type TPMI_SH_POLICY.
 * @param[in] shandle1 First session handle.
 * @param[in] shandle2 Second session handle.
 * @param[in] shandle3 Third session handle.
 * @param[in] timeout Input parameter of type TPM2B_TIMEOUT.
 * @param[in] cpHashA Input parameter of type TPM2B_DIGEST.
 * @param[in] policyRef Input parameter of type TPM2B_NONCE.
 * @param[in] authName Input parameter of type TPM2B_NAME.
 * @param[in] ticket Input parameter of type TPMT_TK_AUTH.
 * @retval TSS2_RC_SUCCESS on success
 * @retval TSS2_RC_BAD_SEQUENCE if context is not ready for this function
 * \todo add further error RCs to documentation
 */
TSS2_RC
Esys_PolicyTicket_async(
    ESYS_CONTEXT *esysContext,
    ESYS_TR policySession,
    ESYS_TR shandle1,
    ESYS_TR shandle2,
    ESYS_TR shandle3,
    const TPM2B_TIMEOUT *timeout,
    const TPM2B_DIGEST *cpHashA,
    const TPM2B_NONCE *policyRef,
    const TPM2B_NAME *authName,
    const TPMT_TK_AUTH *ticket)
{
    TSS2_RC r = TSS2_RC_SUCCESS;
    TSS2L_SYS_AUTH_COMMAND auths = { 0 };
    RSRC_NODE_T *policySessionNode;

    if (esysContext == NULL) {
        LOG_ERROR("esyscontext is NULL.");
        return TSS2_ESYS_RC_BAD_REFERENCE;
    }
    r = iesys_check_sequence_async(esysContext);
    if (r != TSS2_RC_SUCCESS)
        return r;
    r = check_session_feasability(shandle1, shandle2, shandle3, 0);
    return_if_error(r, "Check session usage");

    store_input_parameters(esysContext, policySession,
                timeout,
                cpHashA,
                policyRef,
                authName,
                ticket);
    r = esys_GetResourceObject(esysContext, policySession, &policySessionNode);
    if (r != TPM2_RC_SUCCESS)
        return r;
    r = Tss2_Sys_PolicyTicket_Prepare(esysContext->sys,
                (policySessionNode == NULL) ? TPM2_RH_NULL : policySessionNode->rsrc.handle,
                timeout,
                cpHashA,
                policyRef,
                authName,
                ticket);
    if (r != TSS2_RC_SUCCESS) {
        LOG_ERROR("Error async PolicyTicket");
        return r;
    }
    r = init_session_tab(esysContext, shandle1, shandle2, shandle3);
    return_if_error(r, "Initialize session resources");

    iesys_compute_session_value(esysContext->session_tab[0], NULL, NULL);
    iesys_compute_session_value(esysContext->session_tab[1], NULL, NULL);
    iesys_compute_session_value(esysContext->session_tab[2], NULL, NULL);
    r = iesys_gen_auths(esysContext, policySessionNode, NULL, NULL, &auths);
    return_if_error(r, "Error in computation of auth values");

    esysContext->authsCount = auths.count;
    r = Tss2_Sys_SetCmdAuths(esysContext->sys, &auths);
    if (r != TSS2_RC_SUCCESS) {
        return r;
    }

    r = Tss2_Sys_ExecuteAsync(esysContext->sys);
    return_if_error(r, "Finish (Execute Async)");

    esysContext->state = _ESYS_STATE_SENT;

    return r;
}

/** Asynchronous finish function for TPM2_PolicyTicket
 *
 * This function returns the results of a TPM2_PolicyTicket command
 * invoked via Esys_PolicyTicket_finish. All non-simple output parameters
 * are allocated by the function's implementation. NULL can be passed for every
 * output parameter if the value is not required.
 *
 * @param[in,out] esysContext The ESYS_CONTEXT.
 * @retval TSS2_RC_SUCCESS on success
 * @retval TSS2_RC_BAD_SEQUENCE if context is not ready for this function.
 * \todo add further error RCs to documentation
 */
TSS2_RC
Esys_PolicyTicket_finish(
    ESYS_CONTEXT *esysContext)
{
    LOG_TRACE("complete");
    if (esysContext == NULL) {
        LOG_ERROR("esyscontext is NULL.");
        return TSS2_ESYS_RC_BAD_REFERENCE;
    }
    if (esysContext->state != _ESYS_STATE_SENT) {
        LOG_ERROR("Esys called in bad sequence.");
        return TSS2_ESYS_RC_BAD_SEQUENCE;
    }
    TSS2_RC r = TSS2_RC_SUCCESS;

    r = Tss2_Sys_ExecuteFinish(esysContext->sys, esysContext->timeout);
    if ((r & ~TSS2_RC_LAYER_MASK) == TSS2_BASE_RC_TRY_AGAIN) {
        LOG_DEBUG("A layer below returned TRY_AGAIN: %" PRIx32, r);
        return r;
    }
    if (r == TPM2_RC_RETRY || r == TPM2_RC_TESTING || r == TPM2_RC_YIELDED) {
        LOG_DEBUG("TPM returned RETRY, TESTING or YIELDED, which triggers a "
            "resubmission: %" PRIx32, r);
        if (esysContext->submissionCount > _ESYS_MAX_SUMBISSIONS) {
            LOG_WARNING("Maximum number of resubmissions has been reached.");
            esysContext->state = _ESYS_STATE_ERRORRESPONSE;
            return r;
        }
        esysContext->state = _ESYS_STATE_RESUBMISSION;
        r = Esys_PolicyTicket_async(esysContext,
                esysContext->in.PolicyTicket.policySession,
                esysContext->session_type[0],
                esysContext->session_type[1],
                esysContext->session_type[2],
                esysContext->in.PolicyTicket.timeout,
                esysContext->in.PolicyTicket.cpHashA,
                esysContext->in.PolicyTicket.policyRef,
                esysContext->in.PolicyTicket.authName,
                esysContext->in.PolicyTicket.ticket);
        if (r != TSS2_RC_SUCCESS) {
            LOG_ERROR("Error attempting to resubmit");
            return r;
        }
        r = TSS2_ESYS_RC_TRY_AGAIN;
        return r;
    }
    if (r != TSS2_RC_SUCCESS) {
        LOG_ERROR("Error finish (ExecuteFinish) PolicyTicket");
        return r;
    }
    /*
     * Now the verification of the response (hmac check) and if necessary the
     * parameter decryption have to be done
     */
    const uint8_t *rpBuffer;
    size_t rpBuffer_size;
    TSS2L_SYS_AUTH_RESPONSE rspAuths = {0};
    HASH_TAB_ITEM rp_hash_tab[3];
    HASH_TAB_ITEM rp_hash_tab2[3];
    uint8_t rpHashNum = 0;
    uint8_t rpHashNum2 = 0;
    r = Tss2_Sys_GetRspAuths(esysContext->sys, &rspAuths);
    if (r != TSS2_RC_SUCCESS)
        return r;

    if (rspAuths.count != esysContext->authsCount) {
        LOG_ERROR("Number of response auths differs: %i (expected %i)",
                rspAuths.count, esysContext->authsCount);
        r = TSS2_ESYS_RC_GENERAL_FAILURE;
        return r;
    }
    /*
     * At least one session object is defined so the rp hashes must be computed
     * and the HMACs of the responses have to be checked.
     * Encrypted response parameters will be decrypted.
     */
    if (esysContext->session_type[0] >= ESYS_TR_MIN_OBJECT ||
        esysContext->session_type[1] >= ESYS_TR_MIN_OBJECT ||
        esysContext->session_type[2] >= ESYS_TR_MIN_OBJECT) {
        r = Tss2_Sys_GetRpBuffer(esysContext->sys, &rpBuffer_size, &rpBuffer);
        return_if_error(r, "Error: get rp buffer");

        r = iesys_compute_rp_hashtab(esysContext,
                                     &rspAuths, rpBuffer, rpBuffer_size,
                                     &rp_hash_tab[0], &rpHashNum);
        return_if_error(r, "Error: while computing response hashes");

        r = iesys_check_rp_hmacs(esysContext, &rspAuths, &rp_hash_tab[0]);
        return_if_error(r, "Error: response hmac check");
        if (esysContext->encryptNonce != NULL) {
            r = iesys_decrypt_param(esysContext, rpBuffer, rpBuffer_size);
            return_if_error(r, "Error: while decrypting parameter.");
        }
    }
    /*
     * After the verification of the response we call the complete function
     * to deliver the result.
     */
    r = Tss2_Sys_PolicyTicket_Complete(esysContext->sys);
    if (r != TSS2_RC_SUCCESS) {
        LOG_ERROR("Error finish (ExecuteFinish) PolicyTicket: %" PRIx32, r);
        esysContext->state = _ESYS_STATE_ERRORRESPONSE;
        return r;;
    }
    esysContext->state = _ESYS_STATE_FINISHED;

    return r;
}