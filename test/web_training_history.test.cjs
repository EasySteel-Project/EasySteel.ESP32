const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const source = fs.readFileSync(path.join(__dirname, '../include/network/training_page.h'), 'utf8');
const begin = source.indexOf('    function recordTrainingResult(');
const end = source.indexOf('    function setHistoryFilter(', begin);
function harness() {
  const sandbox = {
    history: [], lastRecordedRoundId: '', selectedStageId: 'stage-one', renders: 0,
    TEXT: { modeCompetition: 'Competition', modeAllTargets: 'All Targets', modeDrill: 'Reaction Drill' },
    localStorage: { setItem() {} },
  };
  sandbox.loadHistory = () => sandbox.history;
  sandbox.saveHistory = history => { sandbox.history = history; };
  sandbox.renderHistoryUI = () => { sandbox.renders++; };
  vm.createContext(sandbox);
  vm.runInContext(source.slice(begin, end), sandbox);
  return sandbox;
}
const run = { id: 'one', timeMs: 13000, rawTimeMs: 1000, mode: 0, penaltyCount: 4, missCount: 4 };
test('penalty correction updates one history entry while preserving stage and timestamp', () => {
  const h = harness();
  h.recordTrainingResult(run);
  const timestamp = h.history[0].timestamp;
  h.selectedStageId = 'stage-two';
  h.recordTrainingResult({ ...run, timeMs: 16000, penaltyCount: 5 });
  assert.equal(h.history.length, 1);
  assert.equal(h.history[0].finalTimeMs, 16000);
  assert.equal(h.history[0].penaltyCount, 5);
  assert.equal(h.history[0].timestamp, timestamp);
  assert.equal(h.history[0].stageId, 'stage-one');
  h.recordTrainingResult({ ...run, timeMs: 16000, penaltyCount: 5 });
  assert.equal(h.renders, 2);
});
test('polling does not restore an explicitly deleted entry', () => {
  const h = harness();
  h.recordTrainingResult(run);
  h.history = [];
  h.recordTrainingResult({ ...run, timeMs: 16000, penaltyCount: 5 });
  assert.equal(h.history.length, 0);
  h.recordTrainingResult({ ...run, id: 'two' });
  assert.equal(h.history.length, 1);
});
