# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

import mxnet as mx
import numpy as np
from mxnet import gluon
from common import setup_module, with_seed, teardown
from mxnet.gluon import nn
from mxnet.base import MXNetError
from mxnet.test_utils import assert_exception, default_context, set_default_context
from nose.tools import assert_raises

@with_seed()
def test_exc_imperative():
    def imperative(exec_numpy=True):
        a = mx.nd.random.normal(0, 1, (2, 2))
        b = mx.nd.random.normal(0, -1, (2, 2))
        c = mx.nd.dot(a, b)
        if exec_numpy:
            c.asnumpy()

    imperative(exec_numpy=False)
    assert_raises(MXNetError, imperative, exec_numpy=True)

@with_seed()
def test_exc_symbolic():
    def symbolic(exec_backward=True, waitall=True):
        x = mx.sym.Variable('x')
        y = mx.sym.Variable('y')
        z = mx.sym.Variable('z')
        x_shape = (2, 2)
        z_shape = (3, 2)
        inputs = [x, y]
        out = mx.symbol.ElementWiseSum(*inputs, name="esum")
        out = mx.sym.dot(z, out)
        out2 = mx.sym.random.normal(0, -1, x_shape, ctx=default_context())
        out = mx.sym.dot(out, out2)
        out = mx.sym.make_loss(out)
        arr = {'x': mx.nd.random.normal(0, 1, x_shape, ctx=default_context()),
               'y': mx.nd.random.normal(0, 1, x_shape, ctx=default_context()),
               'z': mx.nd.random.normal(0, 1, z_shape, ctx=default_context())}
        arr_grad = {'x': mx.nd.empty(x_shape), 'y': mx.nd.empty(x_shape), 'z': mx.nd.empty(z_shape)}
        exec1 = out.bind(ctx=default_context(), args=arr, args_grad=arr_grad)
        outputs = exec1.forward()
        if exec_backward:
            exec1.backward()
            if waitall:
                mx.nd.waitall()
            else:
                exec1.grad_arrays[0].asnumpy()
        else:
            if waitall:
                mx.nd.waitall()
            else:
                outputs[0].asnumpy()

    assert_raises(MXNetError, symbolic, exec_backward=False)
    assert_raises(MXNetError, symbolic, exec_backward=True)

    assert_raises(MXNetError, symbolic, exec_backward=False, waitall=True)
    assert_raises(MXNetError, symbolic, exec_backward=True, waitall=True)

@with_seed()
def test_exc_gluon():
    def gluon(exec_wait=True, waitall=False):
        model = nn.Sequential()
        model.add(nn.Dense(128, activation='tanh', in_units=10, flatten=False))
        model.add(nn.Dropout(1))
        model.add(nn.Dense(64, activation='tanh', in_units=256),
                  nn.Dense(32, in_units=64))
        x = mx.sym.var('data')
        y = model(x)
        model.collect_params().initialize(ctx=[default_context()])
        z = model(mx.nd.random.normal(10, -10, (32, 2, 10), ctx=default_context()))
        if waitall:
            mx.nd.waitall()
        elif exec_wait:
            z.wait_to_read()

    gluon(exec_wait=False)
    assert_raises(MXNetError, gluon, exec_wait=True)

    assert_raises(MXNetError, gluon, waitall=True)

@with_seed()
def test_exc_multiple_waits():
    def multiple_waits(waitall=False):
        # Test calling failed op followed by wait_to_read or waitall twice
        # Intention is to test rethrow for multiple wait_to_reads and waitalls
        # for vars with exceptions in same scope
        caught = False
        try:
            a = mx.nd.random.normal(0, -1, (2, 2)).copyto(default_context())
            if waitall:
                mx.nd.waitall()
            else:
                a.wait_to_read()
        except MXNetError:
            caught = True
        assert caught, "No exception thrown, exception should be rethrown with wait_to_read/waitall"
        try:
            b = mx.nd.random.normal(0, -1, (2, 2)).copyto(default_context())
            if waitall:
                mx.nd.waitall()
            else:
                b.wait_to_read()
        except MXNetError:
            caught = True
        assert caught, "No exception thrown, exception should be rethrown with wait_to_read/waitall"

    multiple_waits(waitall=False)
    multiple_waits(waitall=True)

@with_seed()
def test_exc_post_fail():
    def post_fail(waitall=False):
        caught = False
        try:
            a, b = mx.nd.random_normal(0, -1, (2, 2)).copyto(default_context())
            if waitall:
                mx.nd.waitall()
            else:
                a.asnumpy()
        except MXNetError:
            caught = True
        assert caught, "No exception thrown"
        b.asnumpy()
    post_fail(waitall=False)
    post_fail(waitall=True)

@with_seed()
def test_exc_mutable_var_fail():
    def mutable_var_check(waitall=False):
        a, b = mx.nd.random_normal(0, -1, (2, 2)).copyto(default_context())
        a = mx.nd.dot(a, a)
        if waitall:
            mx.nd.waitall()
        else:
            a.asnumpy()
    assert_raises(MXNetError, mutable_var_check, waitall=False)
    assert_raises(MXNetError, mutable_var_check, waitall=True)

@with_seed()
def test_multiple_waitalls():
    caught = False
    try:
        a = mx.nd.random.normal(0, -1, (2, 2)).copyto(default_context())
        mx.nd.waitall()
    except MXNetError:
        caught = True
    assert caught, "No exception thrown"
    mx.nd.waitall()

@with_seed()
def run_training_iteration(data):
    output = net(data)

    net = gluon.nn.HybridSequential()
    with net.name_scope():
        net.add(gluon.nn.Dense(10))

    ctx = default_context()
    net.collect_params().initialize(mx.init.Xavier(), ctx=ctx)
    data = mx.nd.ones((3, 4))
    mx.profiler.set_state("run")
    run_training_iteration(data)
    mx.nd.waitall()
    mx.profiler.set_state("stop")

@with_seed()
def test_opencv_exception():
    def check_resize():
        img = mx.nd.ones((1200, 1600, 3))
        img = mx.image.imresize(img, 320, 320, interp=-1)
        img.asnumpy()
    assert_raises(MXNetError, check_resize)


@with_seed()
def test_np_reshape_exception():
    a = mx.np.ones((10, 10))
    a.reshape((-1,)).asnumpy()  # Check no-raise
    assert_raises(MXNetError, lambda: a.reshape((1,)))
    assert_raises(MXNetError, lambda: mx.np.reshape(a, (1,)))
    assert_raises(MXNetError, lambda: mx.np.reshape(a, (-1, 3)))


if __name__ == '__main__':
    import nose
    nose.runmodule()
