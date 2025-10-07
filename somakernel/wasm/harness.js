import SomakernelModule from './somakernel.js';

SomakernelModule().then(Module => {
  const init = Module._somakernel_init;
  const submit = Module._somakernel_submit;
  const drain = Module._somakernel_drain;
  const free = Module._somakernel_free;

  const busPtr = init(1024, 2048);

  const encoder = new TextEncoder();
  const msg = encoder.encode("Capsule from JS");

  const msgPtr = Module._malloc(msg.length);
  Module.HEAPU8.set(msg, msgPtr);

  submit(busPtr, msgPtr, msg.length);
  drain(busPtr);
  free(busPtr);
});