// Offline fixtures cover fork PR API reads and the stable check's skipped-needs contract.
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { selectInstallers, checkResults } = require('../../.github/scripts/preview-installers.cjs');
const fixtures = require('./fixtures/preview-installers.json');

for (const fixture of fixtures) {
  test(fixture.name, async () => {
    const files = [
      ...Array.from({ length: fixture.sourceFileCount || 0 }, (_, index) => `src/Test${index}.cpp`),
      ...(fixture.files || []),
    ].map(file => typeof file === 'string' ? { filename: file, status: 'modified' } : file);
    const pullRequest = {
      number: 123,
      head: { sha: 'head', repo: { full_name: 'contributor/mumble' } },
      base: { sha: 'base', repo: { full_name: 'densestjarl/mumble' } },
      changed_files: Object.hasOwn(fixture, 'changedFiles') ? fixture.changedFiles : files.length,
      labels: (fixture.labels || []).map(name => ({ name })),
    };
    const context = {
      eventName: fixture.eventName || 'pull_request',
      repo: { owner: 'densestjarl', repo: 'mumble' },
      payload: { action: fixture.action || 'synchronize', pull_request: pullRequest },
    };
    if (context.eventName !== 'pull_request') {
      context.payload = {};
    }

    let paginated = false;
    const github = {
      rest: {
        pulls: {
          listFiles: Symbol('listFiles'),
          async get(parameters) {
            assert.deepEqual(parameters, { owner: 'densestjarl', repo: 'mumble', pull_number: 123 });
            if (fixture.apiError === 'get') {
              throw new Error('GitHub unavailable');
            }
            return { data: { ...pullRequest, ...fixture.current } };
          },
        },
      },
      async paginate(method, parameters) {
        paginated = true;
        assert.equal(method, github.rest.pulls.listFiles);
        assert.deepEqual(parameters, {
          owner: 'densestjarl', repo: 'mumble', pull_number: 123, per_page: 100,
        });
        if (fixture.apiError === 'paginate') {
          throw new Error('Resource not accessible by integration');
        }
        // Model Octokit's flattening of every page; critical inputs can be on later pages.
        const pages = [];
        for (let offset = 0; offset < files.length; offset += parameters.per_page) {
          pages.push(files.slice(offset, offset + parameters.per_page));
        }
        if (fixture.expectedPages) {
          assert.equal(pages.length, fixture.expectedPages);
        }
        return pages.flat();
      },
    };

    const result = await selectInstallers({ github, context });
    assert.equal(result.build, fixture.build);
    assert.equal(paginated, fixture.expectApi !== false);
    assert.ok(result.reason.length > 0);
  });
}

function jobResults(selection) {
  const result = selection === 'true' ? 'success' : 'skipped';
  return {
    'select-installers': { result: 'success', outputs: { 'build-installers': selection } },
    'release-tests': { result: 'success' },
    windows: { result }, macos: { result }, ubuntu: { result }, debian: { result },
    'safety-qr': { result },
  };
}

test('source-only PR has a successful stable check when packaging and QR needs are skipped', () => {
  assert.match(checkResults('pull_request', jobResults('false')), /intentionally skipped/);
});

for (const event of ['pull_request', 'push', 'workflow_dispatch']) {
  test(`${event} succeeds after every installer gate succeeds`, () => {
    assert.match(checkResults(event, jobResults('true')), /gates passed/);
  });
}

for (const event of ['push', 'workflow_dispatch']) {
  test(`${event} cannot accept intentionally skipped packaging`, () => {
    assert.throws(() => checkResults(event, jobResults('false')), /must build every installer/);
  });
}

for (const selection of ['true', 'false']) {
  for (const job of Object.keys(jobResults(selection))) {
    for (const result of ['failure', 'cancelled', selection === 'true' ? 'skipped' : 'success']) {
      if (result === 'success' && ['release-tests', 'select-installers'].includes(job)) {
        continue;
      }
      test(`${selection}: ${job} ${result} cannot turn the stable check green`, () => {
        const needs = jobResults(selection);
        needs[job].result = result;
        assert.throws(() => checkResults('pull_request', needs), new RegExp(job));
      });
    }
  }
}

test('missing selector output cannot silently skip installer gates', () => {
  assert.throws(() => checkResults('pull_request', jobResults(undefined)), /Invalid installer selection/);
});
