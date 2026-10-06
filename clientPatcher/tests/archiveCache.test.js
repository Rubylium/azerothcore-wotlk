const assert = require('assert/strict');
const crypto = require('crypto');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { archiveCache } = require('../../localTools/mpq-builder/archiveCache');

const root = fs.mkdtempSync(path.join(os.tmpdir(), 'client-mpq-cache-'));
const source = path.join(root, 'input');
const output = path.join(root, 'patch.MPQ');
const files = [{ source, archive: 'Interface\\test.lua' }];
const stateName = crypto.createHash('sha256').update(path.resolve(output)).digest('hex') + '.json';
const statePath = path.resolve(__dirname, '../.build/cache/mpq', stateName);
const previousForce = process.env.EVOLUTIONS_FORCE_REBUILD;
try {
    delete process.env.EVOLUTIONS_FORCE_REBUILD;
    fs.writeFileSync(source, 'aaa');
    fs.writeFileSync(output, 'archive');
    assert.equal(archiveCache(output, files, __filename).current, false);
    archiveCache(output, files, __filename).save();
    assert.equal(archiveCache(output, files, __filename).current, true);
    fs.writeFileSync(source, 'bbb');
    assert.equal(archiveCache(output, files, __filename).current, false);
    fs.writeFileSync(source, 'aaa');
    assert.equal(archiveCache(output, [{ source, archive: 'renamed.lua' }], __filename).current, false);
    fs.writeFileSync(output, 'corrupt');
    assert.equal(archiveCache(output, files, __filename).current, false);
    fs.writeFileSync(output, 'archive');
    process.env.EVOLUTIONS_FORCE_REBUILD = '1';
    assert.equal(archiveCache(output, files, __filename).current, false);
    delete process.env.EVOLUTIONS_FORCE_REBUILD;
    fs.unlinkSync(output);
    assert.equal(archiveCache(output, files, __filename).current, false);
    console.log('All MPQ cache tests passed.');
} finally {
    if (previousForce === undefined) delete process.env.EVOLUTIONS_FORCE_REBUILD;
    else process.env.EVOLUTIONS_FORCE_REBUILD = previousForce;
    fs.rmSync(statePath, { force: true });
    const resolved = path.resolve(root);
    assert.ok(resolved.startsWith(path.resolve(os.tmpdir()) + path.sep));
    assert.ok(path.basename(resolved).startsWith('client-mpq-cache-'));
    fs.rmSync(resolved, { recursive: true, force: true });
}
