from pathlib import Path

root = Path(__file__).resolve().parents[2]
source = (root / "app/src/main/cpp/experiments/csharp/SourceLoader.cpp").read_text()
runtime = (root / "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.cpp").read_text()
header = (root / "app/src/main/cpp/experiments/csharp/AutoSourceRuntime.h").read_text()
content = (root / "app/src/main/cpp/experiments/csharp/NativeContent.cpp").read_text()

assert 'runSourcePublicationOnGameThread(' in header
assert 'runSourcePublicationOnGameThread(' in runtime
assert 'pumpSourcePublicationsOnGameThread();' in runtime
assert 'void pumpSourcePublicationsOnGameThread()' in runtime
assert 'g_sourceTextureQueue.push_back(job)' in runtime
assert 'g_sourceTextureQueue.pop_front()' in runtime
assert 'job->cancelled=true;' in runtime
assert 'job->condition.wait_for(lock,std::chrono::seconds(30)' in runtime
assert 'job->condition.wait(lock,[&] {return job->done;});' in runtime
assert '"Gore",[&] {' in source
assert '"Tree",[&] {' in source
assert 'return publishManagedNativeGores(' in source
assert 'return publishManagedNativeTrees(' in source

gore = content.split('bool publishManagedNativeGores(',1)[1].split('bool publishManagedNativeTrees(',1)[0]
tree = content.split('bool publishManagedNativeTrees(',1)[1].split('int registerNativeContent(',1)[0]
assert 'runtime::content::loadTextureAsset(' in gore
assert 'runtime::content::loadTextureAsset(' in tree
assert 'gore::adoptDeclared(' in gore
assert 'runtime::configureModTreeEffects(' in tree
assert 'runtime::configureModTreeShake(' in tree
assert 'runtime::configureModTreeDrops(' in tree

print('CSHARP_GORE_TREE_GAME_THREAD_PASS')
