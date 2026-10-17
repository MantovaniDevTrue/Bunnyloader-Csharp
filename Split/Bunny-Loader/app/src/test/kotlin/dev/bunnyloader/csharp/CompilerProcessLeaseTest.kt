package dev.bunnyloader.csharp

import org.junit.Assert.*
import org.junit.Test

class CompilerProcessLeaseTest {
    @Test fun matchingTerminalRequestReleasesBinding() {
        assertTrue(compilerRequestCompleted("state=finished\nrequestId=new\n", "new"))
        assertTrue(compilerRequestCompleted("state=failed\nrequestId=new\n", "new"))
    }
    @Test fun oldOrIncompleteStatusCannotReleaseNewBinding() {
        assertFalse(compilerRequestCompleted("state=finished\nrequestId=old\n", "new"))
        assertFalse(compilerRequestCompleted("state=compiling\nrequestId=new\n", "new"))
        assertFalse(compilerRequestCompleted("state=finished\nrequestId=newer\n", "new"))
        assertFalse(compilerRequestCompleted("state=finished\n", ""))
    }
}
