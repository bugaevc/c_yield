# Writing generators

The first thing the generator does should be calling `gen_init()`, storing the
returned value (generator handle).

Return type of the function should be declared as `gen_t *`. To actually return,
do `gen_return(handle, val); return handle;`. Never exit the generator without
calling `gen_return()`, even if there's no important value to pass.

To yield, call `gen_yield(handle, val)`. The call returns the value sent via
`gen_send()`, so make sure to save it if it's important.

# Calling generators

Call the generator as a usual function. It won't do anything immediately, but
return the handle you will use to control it.

Interact with the generator by calling `gen_send(handle, val)`. The value you
send will be returned inside the generator from `gen_yield()`, whereas the value
that the generator yields next time will be returned from the `gen_send()` call.
Note: the first call of `gen_send()` has no corresponding `gen_yield()` call to
return value from, so the value you pass will be silently discarded.

When the generator returns, the return value is passed as the result of
`gen_send()` in the same way that yielded values are. Use `gen_is_done()` to
distinguish between yielded and return values. After the generator returns, call
`free()` on the handle.

# Notes

It should work fine to pass handles around and call `gen_*()` on them from
somewhat unexpected places (ex. yield the value from a generator from inside
some other generator that was called by the first one). However, generator
invocations are not reentrant, that is, you should not call `gen_yield()` on a
handle twice without a `gen_send()` call on the same handle in between, and vice
versa.

All `gen_*()` functions are thread-safe.

As `gen_return()` function never returns, putting `return handle;` after it is
merely a convention, doubled as a way to silence the compiler warning about not
returning a `gen_t *` value.

As you could see by reading the source code, `gen_send()` and `gen_yield()` are
actually the same function. What it does is it dumps the state, switches to the
other stack, loads the state saved there and returns the value it got passed in
the first place.

We tried hard to make generators debug-friendly. In particular, we display the
original generator call in the call stack, rather than the place `gen_send()`
was called (especially useful in coroutines running on an event loop).
