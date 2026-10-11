"""Checks that unsafe package pins are rejected before any installation."""
import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import deploy_guest


class ManifestTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.package = Path(self.directory.name) / 'server.deb'
        self.package.write_bytes(b'candidate bytes')
        self.manifest = {'schema': 1, 'source_commit': 'a' * 40,
                         'package_sha256': hashlib.sha256(b'candidate bytes').hexdigest(),
                         'package_version': '1.7.0~preview+git12345678',
                         'os': 'debian-12', 'architecture': 'amd64'}
        self.metadata = '\n'.join(['Package: mumble-server',
                                   'Version: ' + self.manifest['package_version'],
                                   'Architecture: amd64'])

    def test_tampered_package_is_rejected_before_external_commands(self):
        self.package.write_bytes(b'changed bytes')
        with patch.object(deploy_guest, 'run') as command:
            with self.assertRaisesRegex(ValueError, 'digest mismatch'):
                deploy_guest.validate_manifest(self.manifest, self.package)
            command.assert_not_called()

    def test_metadata_must_match_the_package_pin(self):
        for metadata in [self.metadata.replace('amd64', 'arm64'),
                         self.metadata.replace('mumble-server', 'mumble'),
                         self.metadata.replace('1.7.0', '1.8.0')]:
            with self.subTest(metadata=metadata), patch.object(deploy_guest, 'run', return_value=metadata):
                with self.assertRaisesRegex(ValueError, 'metadata'):
                    deploy_guest.validate_manifest(self.manifest, self.package)

    def test_partial_commit_or_wrong_guest_are_rejected(self):
        for field, value in [('source_commit', 'a' * 8), ('os', 'ubuntu-24.04'),
                             ('architecture', 'arm64'), ('package_sha256', 'bad'), ('schema', 2)]:
            with self.subTest(field=field), patch.object(deploy_guest, 'run') as command:
                with self.assertRaises(ValueError):
                    deploy_guest.validate_manifest({**self.manifest, field: value}, self.package)
                command.assert_not_called()

    def test_verified_package_metadata_is_accepted(self):
        with patch.object(deploy_guest, 'run', return_value=self.metadata):
            deploy_guest.validate_manifest(self.manifest, self.package)


class ReadinessTests(unittest.TestCase):
    def test_crash_loop_never_becomes_ready(self):
        times = iter(range(10))
        statuses = [f'ActiveState=active\nNRestarts={i}\nInvocationID=instance-{i}' for i in range(4)]
        with patch.object(deploy_guest, 'run', side_effect=statuses), \
                patch.object(deploy_guest, 'local_protocol_ready'), \
                patch.object(deploy_guest.time, 'sleep'), \
                patch.object(deploy_guest.time, 'monotonic', side_effect=lambda: next(times)):
            with self.assertRaisesRegex(RuntimeError, 'restarting'):
                deploy_guest.wait_ready(64738, Path('certificate'), 'owned.service', timeout=4)

    def test_same_pinned_listener_must_remain_ready(self):
        times = iter([0, 0, 0, 1, 1, 2, 2])
        with patch.object(deploy_guest, 'run', return_value='ActiveState=active\nNRestarts=0\nInvocationID=one'), \
                patch.object(deploy_guest, 'local_protocol_ready') as protocol, \
                patch.object(deploy_guest.time, 'sleep'), \
                patch.object(deploy_guest.time, 'monotonic', side_effect=lambda: next(times)):
            deploy_guest.wait_ready(64738, Path('certificate'), 'owned.service', timeout=4)
        self.assertEqual(protocol.call_count, 3)

    def test_tls_identity_and_protocol_are_both_required(self):
        from unittest.mock import MagicMock
        secure = MagicMock()
        secure.__enter__.return_value = secure
        secure.getpeercert.return_value = b'leaf'
        context = MagicMock()
        context.wrap_socket.return_value = secure
        certificate = MagicMock()
        with patch.object(deploy_guest.ssl, 'create_default_context', return_value=context), \
                patch.object(deploy_guest.ssl, 'PEM_cert_to_DER_cert', return_value=b'leaf'), \
                patch.object(deploy_guest.socket, 'create_connection', return_value=MagicMock()):
            secure.recv.side_effect = [b'\0\0\0\0\0\2', b'\x08\x01']
            deploy_guest.local_protocol_ready(64738, certificate)
            secure.getpeercert.return_value = b'wrong'
            with self.assertRaisesRegex(ValueError, 'TLS identity'):
                deploy_guest.local_protocol_ready(64738, certificate)
            secure.getpeercert.return_value = b'leaf'
            secure.recv.side_effect = [b'\0\3\0\0\0\2']
            with self.assertRaisesRegex(ValueError, 'Version greeting'):
                deploy_guest.local_protocol_ready(64738, certificate)
        self.assertEqual(context.minimum_version, deploy_guest.ssl.TLSVersion.TLSv1_2)
        self.assertFalse(context.check_hostname)



class SSHPortTests(unittest.TestCase):
    def test_requires_configured_live_ssh_listener(self):
        for port, configured, listeners, allowed in [
                (2222, 'port 2222', 'LISTEN 0 128 0.0.0.0:2222 0.0.0.0:* users:(("sshd",pid=1,fd=3))', True),
                (22, 'port 2222', 'LISTEN 0 128 [::]:2222 [::]:* users:(("sshd",pid=1,fd=3))', False),
                (2222, 'port 2222', 'LISTEN 0 128 0.0.0.0:2222 0.0.0.0:* users:(("other",pid=1,fd=3))', False),
                (2222, 'port 22', 'LISTEN 0 128 [::]:2222 [::]:* users:(("sshd",pid=1,fd=3))', False)]:
            with self.subTest(port=port, configured=configured), patch.object(deploy_guest, 'run', side_effect=[configured, listeners]):
                if allowed:
                    deploy_guest.verify_ssh_port(port)
                else:
                    with self.assertRaisesRegex(ValueError, 'firewall unchanged'):
                        deploy_guest.verify_ssh_port(port)

    def test_certificate_verification_is_not_reported_as_crash_loop(self):
        import ssl
        with patch.object(deploy_guest, 'run', return_value='ActiveState=active\nNRestarts=0\nInvocationID=one'), \
                patch.object(deploy_guest, 'local_protocol_ready', side_effect=ssl.SSLCertVerificationError('wrong certificate')):
            with self.assertRaisesRegex(ValueError, 'certificate verification'):
                deploy_guest.wait_ready(64738, Path('certificate'), 'owned.service')

if __name__ == '__main__':
    unittest.main()
