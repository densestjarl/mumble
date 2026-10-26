// Keep the selector read-only: fork pull requests only need the base repository's file list.
const installerLabel = 'ci:installers';
const installerJobs = ['windows', 'macos', 'ubuntu', 'debian', 'safety-qr'];

function isInstallerInput(filename) {
  const prefixes = [
    '.github/workflows/', '.github/actions/', '.github/scripts/', '.ci/',
    'cmake/', '3rdparty/', '3rdPartyLicenses/', 'installer/', 'macx/',
    'auxiliary_files/', 'scripts/', 'icons/', 'themes/', 'samples/',
    'src/mumble/qttranslations/',
  ];
  const basename = filename.split('/').at(-1);
  return prefixes.some(prefix => filename.startsWith(prefix))
    || ['.gitmodules', 'dependencies.json', 'LICENSE'].includes(filename)
    || ['CMakeLists.txt', 'CMakePresets.json', 'CMakeUserPresets.json'].includes(basename)
    || /\.(cmake|in|qrc|rc|manifest|plist|ts)$/.test(basename);
}

function requestsInstallers(pullRequest) {
  return pullRequest.labels?.some(label => label.name.toLowerCase() === installerLabel);
}

async function selectInstallers({ github, context }) {
  if (context.eventName !== 'pull_request') {
    return { build: true, reason: 'Pushes and manual runs build every installer.' };
  }

  const pullRequest = context.payload.pull_request;
  if (requestsInstallers(pullRequest)) {
    return { build: true, reason: `The ${installerLabel} label requests every installer.` };
  }

  // The files API is capped at 3,000 even when paginated. Never skip on an incomplete diff.
  if (!Number.isInteger(pullRequest.changed_files) || pullRequest.changed_files <= 0
      || pullRequest.changed_files > 3000) {
    return { build: true, reason: 'The complete pull request diff cannot be verified.' };
  }

  try {
    const parameters = { ...context.repo, pull_number: pullRequest.number };
    const files = await github.paginate(github.rest.pulls.listFiles, {
      ...parameters, per_page: 100,
    });
    const { data: current } = await github.rest.pulls.get(parameters);

    if (requestsInstallers(current)) {
      return { build: true, reason: `The ${installerLabel} label requests every installer.` };
    }
    // The API lists the current PR, which may have advanced since this workflow was queued.
    if (current.head.sha !== pullRequest.head.sha || current.base.sha !== pullRequest.base.sha
        || files.length !== pullRequest.changed_files || files.length !== current.changed_files) {
      return { build: true, reason: 'The pull request changed or its file list is incomplete.' };
    }
    if (files.some(file => isInstallerInput(file.filename)
        || (file.previous_filename && isInstallerInput(file.previous_filename)))) {
      return { build: true, reason: 'Packaging, dependency, build, or workflow inputs changed.' };
    }
    return {
      build: false,
      reason: `No installer inputs changed; packaging skipped. Add ${installerLabel} to request every installer.`,
    };
  } catch {
    // Read-only tokens work for forks. If API access fails, retain full installer coverage.
    return { build: true, reason: 'Unable to verify installer inputs through the API; building every installer.' };
  }
}

function checkResults(eventName, needs) {
  for (const job of ['select-installers', 'release-tests']) {
    if (needs[job]?.result !== 'success') {
      throw new Error(`${job} did not succeed.`);
    }
  }
  const selection = needs['select-installers'].outputs['build-installers'];
  if (!['true', 'false'].includes(selection) || (selection === 'false' && eventName !== 'pull_request')) {
    throw new Error('Invalid installer selection; pushes and manual runs must build every installer.');
  }
  const expected = selection === 'true' ? 'success' : 'skipped';
  for (const job of installerJobs) {
    if (needs[job]?.result !== expected) {
      throw new Error(`${job}: expected ${expected}, got ${needs[job]?.result}.`);
    }
  }
  return selection === 'true'
    ? 'All installer, regression, installation, release/update, and safety QR gates passed.'
    : `Release/update tests passed. PR packaging intentionally skipped; use ${installerLabel} to request it.`;
}

module.exports = { selectInstallers, checkResults };
