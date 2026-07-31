import fs from 'node:fs';
import vm from 'node:vm';
import { performance } from 'node:perf_hooks';
import { webcrypto } from 'node:crypto';

const context = {
  TextEncoder,
  TextDecoder,
  crypto: webcrypto,
  performance,
  btoa: value => Buffer.from(value, 'binary').toString('base64'),
  atob: value => Buffer.from(value, 'base64').toString('binary'),
};
context.window = context;
vm.runInNewContext(
  fs.readFileSync(new URL('../data/wakeup_import.js', import.meta.url), 'utf8'),
  context,
);

const api = context.WakeUpImport;
if (api.md5('abc') !== '900150983cd24fb0d6963f7d28e17f72') {
  throw new Error('MD5 fixture mismatch');
}
const cipher = api._test.dcrypt(new TextEncoder().encode('12345678'), '@fG2SuLA');
if (Buffer.from(cipher.slice(0, 8)).toString('hex') !== '1ff5b33babcf4d57') {
  throw new Error('WakeUp DES fixture mismatch');
}
const key = api._test.getKey('460', '0123456789');
if (key !== 'd0eb06e1daf1f5ce31c3f8974844a4686a551466a0ed5688c1351cdaee3143a1b561206265d96207424a140059792b8931cd22941af1d9898bc5ba70303e789a') {
  throw new Error('WakeUp key derivation fixture mismatch');
}
console.log('PASS  WakeUp browser MD5, DES and request-key fixtures');
