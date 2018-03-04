package net.yinlight.busybox;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.text.ClipboardManager;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.TextView;
import java.io.InputStream;


public class BusyBoxIDE extends Activity
{
	private TextView status_text = null;
	private TextView pastable_text = null;

	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main);
		status_text = (TextView)findViewById(R.id.status);
		pastable_text = (TextView)findViewById(R.id.pastable);
	}

	public void onResume() {
		super.onResume();
		check_status();
	}

	public boolean onCreateOptionsMenu(Menu menu) {
		getMenuInflater().inflate(R.menu.main_menu, menu);
		return true;
	}

	public boolean onOptionsItemSelected(MenuItem item) {
		switch (item.getItemId()) {
			case R.id.copy:
				copy_path();
				return true;
			case R.id.doc: {
				Intent i = new Intent(Intent.ACTION_VIEW);
				i.setData(Uri.parse(
"http://www.galexander.org/software/simplebusybox"));
				startActivity(i);
			}	return true;
			case R.id.about: {
				about_dialog();
			}	return true;
			default:
				return super.onOptionsItemSelected(item);
		}
	}

	public void copy_path() {
		ClipboardManager cl = (ClipboardManager)
			getSystemService(Context.CLIPBOARD_SERVICE);
		if (cl == null) {
			return;
		}
		cl.setText(path_text());
	}

	public void about_dialog() {
		AlertDialog.Builder b =
			new AlertDialog.Builder(this);
		b.setCancelable(true);
		b.setPositiveButton("OK",
			new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface di, int which) {
			} });
		b.setIcon(android.R.drawable.ic_dialog_info);
		b.setTitle("About");
		b.setMessage(
			"BusyBoxIDE version " + get_version_str() +
			"\nBusyBox version 1.24.2");
		b.show();
	}

	public void install_clicked(View v) {
		(new Thread() { public void run() {
			do_install();
		} }).start();
	}

	private void do_install() {
		try {
			InputStream is = getResources().openRawResource(R.raw.busybox);
			if (is == null) {
				update_status("Error opening busybox resource.");
			} else {
				try {
					native_install(get_path(), is);
				} finally {
					is.close();
				}
			}
		} catch (Exception e) {
			update_status("Exception while unpacking: " +
						e.getMessage());
		}
	}

	private void check_status() {
		(new Thread() { public void run() {
			determine_status(get_path());
		} }).start();
	}

	private void update_status(final String s) {
		runOnUiThread(new Runnable() { public void run() {
			if (status_text != null) {
				status_text.setText(s);
			}
		} });
	}

	private void display_path() {
		runOnUiThread(new Runnable() { public void run() {
			if (pastable_text != null) {
				pastable_text.setText(path_text());
			}
		} });
	}

	private String get_path() {
		return getFilesDir().toString();
	}

	private String path_text() {
		return "export PATH="+get_path()+":$PATH";
	}

	private int get_version() {
		int ret = 0;
		try {
			ret = getPackageManager()
				.getPackageInfo(getPackageName(), 0)
				.versionCode;
		} catch (Exception e) { }
		return ret;
	}

	private String get_version_str() {
		String ret = "";
		try {
			ret = getPackageManager()
				.getPackageInfo(getPackageName(), 0)
				.versionName;
		} catch (Exception e) { }
		return ret;
	}


	private native void determine_status(String path);
	private native void native_install(String path, InputStream is);
	static {
		System.loadLibrary("sbb-jni");
	}
}
