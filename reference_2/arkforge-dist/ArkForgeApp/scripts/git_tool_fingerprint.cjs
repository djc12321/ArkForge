const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const app=path.resolve(__dirname,'..'),sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const libraries={};for(const name of fs.readdirSync(path.join(app,'entry/libs/arm64-v8a')).filter(name=>/^libgit.*\.so$/.test(name)))libraries[name]=sha(path.join(app,'entry/libs/arm64-v8a',name));
const manifest={upstream:'https://www.kernel.org/pub/software/scm/git/git-2.55.0.tar.xz',version:'2.55.0',sourceSha256:'457fdb04dc8728e007d4688695e6912e6f680727920f2a40bf11eacc17505357',
  sourceChanges:[],platformAdapter:'native/git_exec_bridge.c linked with --wrap=execve,--wrap=execvp; private Unix process broker provides clean single-threaded CLI processes; wraps fork, execve, execvp, exit and atexit',adapterSha256:sha(path.join(app,'native/git_exec_bridge.c')),
  processBrokerSha256:sha(path.join(app,'native/git_process_broker.cpp')),protocolSha256:sha(path.join(app,'native/git_process_protocol.h')),launcherSha256:sha(path.join(app,'native/official_cli.cpp')),
  libraries,curl:{version:'8.23.0',source:'https://curl.se/download/curl-8.23.0.tar.xz',sourceSha256:'8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5'},
  caBundleSha256:sha(path.join(app,'entry/src/main/resources/rawfile/git/cacert.pem')),license:'GPL-2.0',buildScript:'native/build_git_https.sh',validation:'WEB-W00: private GitHub HTTPS ls-remote, clone, fsck, fetch, pull and authorized empty-commit push verified 2026-10-23'};
fs.writeFileSync(path.join(app,'entry/src/main/resources/rawfile/compiler/git-manifest.json'),JSON.stringify(manifest,null,2)+'\n');
console.log('Git native library fingerprints updated.');
