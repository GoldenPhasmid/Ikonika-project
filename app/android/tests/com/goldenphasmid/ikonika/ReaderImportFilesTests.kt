package com.goldenphasmid.ikonika

import java.io.IOException
import java.io.InputStream
import java.util.concurrent.Callable
import java.util.concurrent.Executors
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

class ReaderImportFilesTests {
    @get:Rule
    val temporary = TemporaryFolder()

    @Test
    fun copiesKeepExistingBytesAndUseNumberedNames() {
        val directory = temporary.newFolder()
        val original = directory.resolve("book.pdf")
        original.writeText("original")

        val first = ReaderImportFiles.copy(directory, "book.pdf", "first".byteInputStream())
        val second = ReaderImportFiles.copy(directory, "book.pdf", "second".byteInputStream())

        assertEquals("book (1).pdf", first.name)
        assertEquals("book (2).pdf", second.name)
        assertEquals("original", original.readText())
        assertEquals("first", first.readText())
        assertEquals("second", second.readText())
    }

    @Test
    fun unusedNamesAndCompoundExtensionsArePreserved() {
        val directory = temporary.newFolder()
        val first = ReaderImportFiles.copy(directory, "book.fb2.zip", "first".byteInputStream())
        val second = ReaderImportFiles.copy(directory, "book.fb2.zip", "second".byteInputStream())

        assertEquals("book.fb2.zip", first.name)
        assertEquals("book (1).fb2.zip", second.name)
    }

    @Test
    fun concurrentCopiesKeepEveryFile() {
        val directory = temporary.newFolder()
        val executor = Executors.newFixedThreadPool(4)

        try {
            val copies = executor.invokeAll((0 until 12).map { index ->
                Callable {
                    ReaderImportFiles.copy(directory, "book.pdf", "copy-$index".byteInputStream())
                }
            }).map { it.get() }

            assertEquals(12, copies.map { it.name }.toSet().size)
            copies.forEachIndexed { index, file -> assertEquals("copy-$index", file.readText()) }
        } finally {
            executor.shutdownNow()
        }
    }

    @Test
    fun failedCopiesRemoveOnlyTheirOwnFile() {
        val directory = temporary.newFolder()
        val original = directory.resolve("book.pdf")
        original.writeText("original")
        val broken = object : InputStream() {
            override fun read(): Int = throw IOException("Read failed")
        }

        try {
            ReaderImportFiles.copy(directory, "book.pdf", broken)
            assertTrue("The failed read must propagate", false)
        } catch (expected: IOException) {
            assertEquals("Read failed", expected.message)
        }

        assertEquals("original", original.readText())
        assertFalse(directory.resolve("book (1).pdf").exists())
    }
}
