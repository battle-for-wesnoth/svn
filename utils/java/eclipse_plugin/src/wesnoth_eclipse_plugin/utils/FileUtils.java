/**
 * @author Timotei Dolean
 */
package wesnoth_eclipse_plugin.utils;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;

public class FileUtils
{
	/**
	 * Copies a file from source to target
	 * @param source
	 * @param target
	 * @throws Exception
	 */
	public static void copyTo(File source, File target) throws Exception
	{
		if (source == null || target == null)
			return;

		InputStream in = new FileInputStream(source);
		OutputStream out = new FileOutputStream(target);

		// Transfer bytes from in to out
		byte[] buf = new byte[1024];
		int len;
		while ((len = in.read(buf)) > 0) {
			out.write(buf, 0, len);
		}
		in.close();
		out.close();
	}

	public static String getFileContents(File file)
	{
		String contentsString = "";
		BufferedReader reader = null;
		try
		{
			String line = "";
			reader = new BufferedReader(new InputStreamReader(new FileInputStream(file)));
			while((line = reader.readLine()) != null)
				contentsString += (line + "\n");
		}
		catch (Exception e)
		{
			e.printStackTrace();
		}
		finally{
			try{
				reader.close();
			}
			catch (Exception e) {
				e.printStackTrace();
			}
		}
		return contentsString;
	}
}