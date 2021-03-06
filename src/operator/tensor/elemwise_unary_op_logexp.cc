/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 * \file elemwise_unary_op_logexp.cc
 * \brief CPU Implementation of elementwise log and exp function.
 */
#include <mxnet/base.h>
#include "elemwise_unary_op.h"
#include "./elemwise_binary_op-inl.h"
#include "../nn/mkldnn/mkldnn_ops-inl.h"

namespace mxnet {
namespace op {

// exp
#if MSHADOW_USE_MKL == 1
MXNET_MKL_OPERATOR_REGISTER_UNARY_WITH_SPARSE_DR(exp, cpu, mshadow_op::exp, mkl_func::exp)
MXNET_ADD_SPARSE_OP_ALIAS(exp)
.describe(R"code(Returns element-wise exponential value of the input.

.. math::
   exp(x) = e^x \approx 2.718^x

Example::

   exp([0, 1, 2]) = [1., 2.71828175, 7.38905621]

The storage type of ``exp`` output is always dense

)code" ADD_FILELINE)
.set_attr<nnvm::FGradient>("FGradient", ElemwiseGradUseOut{"_mul"});
#else
MXNET_OPERATOR_REGISTER_UNARY_WITH_SPARSE_DR(exp, cpu, mshadow_op::exp)
MXNET_ADD_SPARSE_OP_ALIAS(exp)
.describe(R"code(Returns element-wise exponential value of the input.

.. math::
   exp(x) = e^x \approx 2.718^x

Example::

   exp([0, 1, 2]) = [1., 2.71828175, 7.38905621]

The storage type of ``exp`` output is always dense

)code" ADD_FILELINE)
.set_attr<nnvm::FGradient>("FGradient", ElemwiseGradUseOut{"_mul"});
#endif

// log
MXNET_OPERATOR_REGISTER_UNARY(log)
MXNET_ADD_SPARSE_OP_ALIAS(log)
.describe(R"code(Returns element-wise Natural logarithmic value of the input.

The natural logarithm is logarithm in base *e*, so that ``log(exp(x)) = x``

The storage type of ``log`` output is always dense

)code" ADD_FILELINE)
#if MSHADOW_USE_MKL == 1
.set_attr<FCompute>("FCompute<cpu>", UnaryOp::MKL_Compute<mshadow_op::log, mkl_func::log>)
#else
.set_attr<FCompute>("FCompute<cpu>", UnaryOp::Compute<cpu, mshadow_op::log>)
#endif    // MSHADOW_USE_MKL == 1
.set_attr<nnvm::FGradient>("FGradient", ElemwiseGradUseIn{"_backward_log"});

// log10
MXNET_OPERATOR_REGISTER_UNARY_WITH_SPARSE_DR(log10, cpu, mshadow_op::log10)
MXNET_ADD_SPARSE_OP_ALIAS(log10)
.describe(R"code(Returns element-wise Base-10 logarithmic value of the input.

``10**log10(x) = x``

The storage type of ``log10`` output is always dense

)code" ADD_FILELINE)
.set_attr<nnvm::FGradient>("FGradient", ElemwiseGradUseIn{"_backward_log10"});

// log2
MXNET_OPERATOR_REGISTER_UNARY_WITH_SPARSE_DR(log2, cpu, mshadow_op::log2)
MXNET_ADD_SPARSE_OP_ALIAS(log2)
.describe(R"code(Returns element-wise Base-2 logarithmic value of the input.

``2**log2(x) = x``

The storage type of ``log2`` output is always dense

)code" ADD_FILELINE)
.set_attr<nnvm::FGradient>("FGradient", ElemwiseGradUseIn{"_backward_log2"});

MXNET_OPERATOR_REGISTER_BINARY_WITH_SPARSE_CPU_DR(_backward_log,
                                                  unary_bwd<mshadow_op::log_grad>)
.set_attr<nnvm::FGradient>("FGradient",
  [](const nnvm::NodePtr& n, const std::vector<nnvm::NodeEntry>& ograds) {
    // ograds[0]: dL/dxgrad
    // inputs[0]: dL/dy
    // inputs[1]: x
    // f(x) = y = log(x)
    // f'(x) = 1/x
    // f''(x) = -1 * (f'(x) * f'(x))
    auto dydx_mul_dldy = nnvm::NodeEntry{n};  // f'(x) * head_grads
    auto dlogx = MakeNode("reciprocal", n->attrs.name + "_dlogx",
                            {n->inputs[1]}, nullptr, &n);
    auto d2ydx2_mid = MakeNode("elemwise_mul", n->attrs.name + "_d2ydx2_mid",
                            {dydx_mul_dldy, nnvm::NodeEntry{dlogx}}, nullptr, &n);
    auto d2ydx2 = MakeNode("negative", n->attrs.name + "_d2ydx2",
                        {nnvm::NodeEntry{d2ydx2_mid}}, nullptr, &n);

    std::vector<nnvm::NodeEntry> ret;

    ret.emplace_back(MakeNode("elemwise_mul", n->attrs.name + "_backward_grad_grad",
                             {ograds[0], nnvm::NodeEntry{dlogx}}, nullptr, &n));
    ret.emplace_back(MakeNode("elemwise_mul", n->attrs.name + "_backward_grad_grad_inp",
                             {ograds[0], nnvm::NodeEntry{d2ydx2}}, nullptr, &n));
    return ret;
  });

MXNET_OPERATOR_REGISTER_BINARY_WITH_SPARSE_CPU_DR(_backward_log10,
                                                  unary_bwd<mshadow_op::log10_grad>)
.set_attr<nnvm::FGradient>("FGradient",
  [](const nnvm::NodePtr& n, const std::vector<nnvm::NodeEntry>& ograds) {
    // ograds[0]: dL/dxgrad
    // inputs[0]: dL/dy
    // inputs[1]: x
    // f(x) = y = log10(x)
    // f'(x) = 1 / (log(10) * x)
    // f''(x) = -1 * (f'(x) * 1/x)
    auto dydx_mul_dldy = nnvm::NodeEntry{n};  // f'(x) * head_grads
    auto dydx = MakeNode("elemwise_div", n->attrs.name + "_dydx",
                            {n->inputs[0]}, nullptr, &n);
    auto dlogx = MakeNode("reciprocal", n->attrs.name + "_dlogx",
                            {n->inputs[1]}, nullptr, &n);
    auto d2ydx2_mid = MakeNode("elemwise_mul", n->attrs.name + "_d2ydx2_mid",
                            {dydx_mul_dldy, nnvm::NodeEntry{dlogx}}, nullptr, &n);
    auto d2ydx2 = MakeNode("negative", n->attrs.name + "_d2ydx2",
                        {nnvm::NodeEntry{d2ydx2_mid}}, nullptr, &n);

    std::vector<nnvm::NodeEntry> ret;

    ret.emplace_back(MakeNode("elemwise_mul", n->attrs.name + "_backward_grad_grad",
                             {ograds[0], nnvm::NodeEntry{dydx}}, nullptr, &n));
    ret.emplace_back(MakeNode("elemwise_mul", n->attrs.name + "_backward_grad_grad_inp",
                             {ograds[0], nnvm::NodeEntry{d2ydx2}}, nullptr, &n));
    return ret;
  });

MXNET_OPERATOR_REGISTER_BINARY_WITH_SPARSE_CPU_DR(_backward_log2,
                                                  unary_bwd<mshadow_op::log2_grad>)
.set_attr<nnvm::FGradient>("FGradient",
  [](const nnvm::NodePtr& n, const std::vector<nnvm::NodeEntry>& ograds) {
    // ograds[0]: dL/dxgrad
    // inputs[0]: dL/dy
    // inputs[1]: x
    // f(x) = y = log2(x)
    // f'(x) = 1 / (log(2) * x)
    // f''(x) = -1 * (f'(x) * 1/x)
    auto dydx_mul_dldy = nnvm::NodeEntry{n};  // f'(x) * head_grads
    auto dydx = MakeNode("elemwise_div", n->attrs.name + "_dydx",
                            {n->inputs[0]}, nullptr, &n);
    auto dlogx = MakeNode("reciprocal", n->attrs.name + "_dlogx",
                            {n->inputs[1]}, nullptr, &n);
    auto d2ydx2_mid = MakeNode("elemwise_mul", n->attrs.name + "_d2ydx2_mid",
                            {dydx_mul_dldy, nnvm::NodeEntry{dlogx}}, nullptr, &n);
    auto d2ydx2 = MakeNode("negative", n->attrs.name + "_d2ydx2",
                        {nnvm::NodeEntry{d2ydx2_mid}}, nullptr, &n);

    std::vector<nnvm::NodeEntry> ret;

    ret.emplace_back(MakeNode("elemwise_mul", n->attrs.name + "_backward_grad_grad",
                             {ograds[0], nnvm::NodeEntry{dydx}}, nullptr, &n));
    ret.emplace_back(MakeNode("elemwise_mul", n->attrs.name + "_backward_grad_grad_inp",
                             {ograds[0], nnvm::NodeEntry{d2ydx2}}, nullptr, &n));
    return ret;
  });

// log1p
MXNET_OPERATOR_REGISTER_UNARY_WITH_RSP_CSR(log1p, cpu, mshadow_op::log1p)
.describe(R"code(Returns element-wise ``log(1 + x)`` value of the input.

This function is more accurate than ``log(1 + x)``  for small ``x`` so that
:math:`1+x\approx 1`

The storage type of ``log1p`` output depends upon the input storage type:

   - log1p(default) = default
   - log1p(row_sparse) = row_sparse
   - log1p(csr) = csr

)code" ADD_FILELINE)
.set_attr<nnvm::FGradient>("FGradient", ElemwiseGradUseIn{"_backward_log1p"});

MXNET_OPERATOR_REGISTER_BINARY_WITH_SPARSE_CPU_DR(_backward_log1p,
                                                  unary_bwd<mshadow_op::log1p_grad>);

// expm1
MXNET_OPERATOR_REGISTER_UNARY_WITH_RSP_CSR(expm1, cpu, mshadow_op::expm1)
.describe(R"code(Returns ``exp(x) - 1`` computed element-wise on the input.

This function provides greater precision than ``exp(x) - 1`` for small values of ``x``.

The storage type of ``expm1`` output depends upon the input storage type:

   - expm1(default) = default
   - expm1(row_sparse) = row_sparse
   - expm1(csr) = csr

)code" ADD_FILELINE)
.set_attr<nnvm::FGradient>("FGradient", ElemwiseGradUseIn{"_backward_expm1"});

MXNET_OPERATOR_REGISTER_BINARY_WITH_SPARSE_CPU_DR(_backward_expm1, unary_bwd<mshadow_op::exp>);

}  // namespace op
}  // namespace mxnet
