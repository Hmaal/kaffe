/*
 * Java core library component.
 *
 * Copyright (c) 1997, 1998
 *      Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file.
 */

package java.util.zip;

import java.io.IOException;
import java.io.OutputStream;
import java.util.Enumeration;
import java.util.Vector;
import kaffe.util.UTF8;

// Reference: ftp://ftp.uu.net/pub/archiving/zip/doc/appnote-970311-iz.zip

public class ZipOutputStream extends DeflaterOutputStream
	implements ZipConstants {

  public static final int STORED = Deflater.NO_COMPRESSION;
  public static final int DEFLATED = Deflater.DEFLATED;

private static final int ZIPVER_1_0 = 0x000a;
private static final int ZIPVER_2_0 = 0x0014;

private int method = Deflater.DEFLATED;
private int level = Deflater.DEFAULT_COMPRESSION;
private ZipEntry curr;
private Vector dir;
private OutputStream strm;
private int dout;
private CRC32 crc;

class Storer
  extends Deflater {

private int total;
private byte[] buf;
private int off;
private int len;

Storer() {
    total = 0;
    off = 0;
    len = 0;
}

public int deflate(byte[] b, int p, int l) {
	if (l >= len) {
		l = len;
	}
	System.arraycopy(buf, off, b, p, l);
	total += l;
	off += l;
	len -= l;
	return (l);
}

public synchronized void setInput(byte b[], int o, int l)
{
	buf = b;
	off = o;
	len = l;
}

public boolean needsInput() {
	return (len > 0 ? false : true);
}

public boolean finished() {
	return (len > 0 ? false : true);
}

public int getTotalIn() {
	return (total);
}

public int getTotalOut() {
	return (total);
}

public void reset() {
}

}

public ZipOutputStream(OutputStream out)
{
	super(out);
	strm = out;
	curr = null;
	dir = new Vector();
	dout = 0;
	crc = new CRC32();
}

public void close() throws IOException
{
	finish();
	super.close();
}

public void closeEntry() throws IOException
{
	if (curr == null) {
		return;
	}

	super.finish();
	int in = def.getTotalIn();
	int out = def.getTotalOut();
	long crcval = crc.getValue();
	def.reset();
	crc.reset();

	if (curr.csize != -1 && curr.csize != out) {
		throw new ZipException("compress size set incorrectly");
	}
	if (curr.size != -1 && curr.size != in) {
		throw new ZipException("uncompress size set incorrectly");
	}
	if (curr.crc != -1 && curr.crc != crcval) {
		throw new ZipException("crc set incorrectly");
	}

	curr.csize = out;
	curr.size = in;
	curr.crc = crcval;

	dout += curr.csize;

	// We only add the data descriptor when writing a compressed entry

	if (curr.flag == 0x0008) {
	    byte[] da = new byte[EXTHDR];
	    put32(da, 0, (int)EXTSIG);
	    put32(da, EXTCRC, (int)curr.crc);
	    put32(da, EXTSIZ, (int) curr.csize);
	    put32(da, EXTLEN, (int) curr.size);
	    strm.write(da);
	    dout += EXTHDR;
	}

	curr = null;
}

public void finish() throws IOException {
	byte[] ch = new byte[CENHDR];
	int count = 0;
	int size = 0;

	if (dir == null) {
		return;
	}

	closeEntry();

	// Write the central directory
	for (Enumeration e = dir.elements(); e.hasMoreElements(); ) {
		ZipEntry ze = (ZipEntry)e.nextElement();

		// Convert name to UTF-8 binary
		byte[] nameBuf = (ze.name != null) ?
		    UTF8.encode(ze.name) : new byte[0];

		// Write central directory entry
		put32(ch, 0, (int)CENSIG);
		int zipver = (ze.method == STORED ? ZIPVER_1_0 : ZIPVER_2_0);
		put16(ch, CENVEM, zipver);
		put16(ch, CENVER, zipver);
		put16(ch, CENFLG, ze.flag);
		put16(ch, CENHOW, ze.method);
		put32(ch, CENTIM, ze.dosTime);
		put32(ch, CENCRC, (int)ze.crc);
		put32(ch, CENSIZ, (int)ze.csize);
		put32(ch, CENLEN, (int)ze.size);
		put16(ch, CENNAM, nameBuf.length);
		put16(ch, CENEXT, ze.extra == null ?
			0 : ze.extra.length);
		put16(ch, CENCOM, ze.comment == null ?
			0 : ze.comment.length());
		put16(ch, CENDSK, 0);
		put16(ch, CENATT, 0);
		put32(ch, CENATX, 0);
		put32(ch, CENOFF, (int)ze.offset);

		strm.write(ch);
		size += CENHDR;

		// Write name
		strm.write(nameBuf);
		size += nameBuf.length;

		// Write any extra stuff
		if (ze.extra != null) {
		    strm.write(ze.extra);
		    size += ze.extra.length;
		}

		count++;
	}

	// Flag error if no entries were written.
	if (count == 0) {
	        throw new ZipException("ZIP file must have at least one entry");
	}

	byte[] ce = new byte[ENDHDR];
	put32(ce, 0, (int)ENDSIG);
	put16(ce, ENDNRD, 0);
	put16(ce, ENDDCD, 0);
	put16(ce, ENDSUB, count);
	put16(ce, ENDTOT, count);
	put32(ce, ENDSIZ, size);
	put32(ce, ENDOFF, dout);
	put16(ce, ENDCOM, 0);

	strm.write(ce);

	dir = null;
}

public void putNextEntry(ZipEntry ze) throws IOException
{
	closeEntry();	// Close previous entry

	if (ze.method == -1) {
		ze.method = method;
	}
	if (ze.method == STORED) {
		if (ze.size == -1) {
			throw new ZipException("size not set in stored entry");
		}
		ze.csize = ze.size;
		if (ze.crc == -1) {
			throw new ZipException("crc not set in stored entry");
		}
		ze.flag = 0;
	} else {
		ze.flag = 0x0008;
	}

	if (curr == null || curr.method != ze.method) {
		if (ze.method == STORED) {
			def = new Storer();
		}
		else {
			def = new Deflater(level, true);
		}
	}

	// Convert name to UTF-8 binary
	byte[] nameBuf = (ze.name != null) ?
	    UTF8.encode(ze.name) : new byte[0];

	byte[] lh = new byte[LOCHDR];
	put32(lh, 0, (int)LOCSIG);
	put16(lh, LOCVER,
		ze.method == STORED ? ZIPVER_1_0 : ZIPVER_2_0);
	put16(lh, LOCFLG, ze.flag);
	put16(lh, LOCHOW, ze.method);
	put32(lh, LOCTIM, ze.dosTime);

	if (ze.method == STORED) {
		put32(lh, LOCCRC, (int)ze.crc);
		put32(lh, LOCSIZ, (int)ze.csize);
		put32(lh, LOCLEN, (int)ze.size);
	} else {
		put32(lh, LOCCRC, 0);
		put32(lh, LOCSIZ, 0);
		put32(lh, LOCLEN, 0);
	}

	put16(lh, LOCNAM, nameBuf.length);
	put16(lh, LOCEXT, ze.extra == null ? 0 : ze.extra.length);

	strm.write(lh);

	ze.offset = dout;
	dout += LOCHDR;

	// Write name
	strm.write(nameBuf);
	dout += nameBuf.length;

	// Write any extra stuff
	if (ze.extra != null) {
		strm.write(ze.extra);
		dout += ze.extra.length;
	}

	// Add entry to list of entries we need to write at the end.
	dir.addElement(ze);

	curr = ze;
}

public void setComment(String comment)
{
	// Currently ignored.
}

public void setLevel(int lvl)
{
	if ((lvl < Deflater.NO_COMPRESSION || lvl > Deflater.BEST_COMPRESSION)
	    && lvl != Deflater.DEFAULT_COMPRESSION) {
		throw new IllegalArgumentException("bad compression level");
	}
	level = lvl;
}

public void setMethod(int m)
{
	if (m != DEFLATED && m != STORED) {
		throw new IllegalArgumentException("bad compression method");
	}
	method = m;
}

public synchronized void write(byte[] buf, int off, int len) throws IOException
{
	super.write(buf, off, len);
	crc.update(buf, off, len);
}

private void put16(byte[] zheader, int pos, int val) {
	zheader[pos] = (byte)val;
	zheader[pos+1] = (byte)(val >>> 8);
}

private void put32(byte[] zheader, int pos, int val) {
	zheader[pos] = (byte)val;
	zheader[pos+1] = (byte)(val >>> 8);
	zheader[pos+2] = (byte)(val >>> 16);
	zheader[pos+3] = (byte)(val >>> 24);
}

}
