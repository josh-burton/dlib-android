// Copyright (c) 2016-present boyw165
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
//    The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

package com.my.demo.dlib;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.my.core.protocol.IProgressBarView;
import com.my.core.util.ViewUtil;
import com.my.demo.dlib.util.DlibModelHelper;
import com.my.widget.adapter.SampleMenuAdapter;
import com.my.widget.adapter.SampleMenuAdapter.SampleMenuItem;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import butterknife.BindView;
import butterknife.ButterKnife;
import butterknife.Unbinder;

public class StartActivity
    extends AppCompatActivity
    implements IProgressBarView {

    @BindView(R.id.toolbar)
    Toolbar mToolbar;
    @BindView(R.id.menu)
    ListView mStartMenu;

    // Butter Knife.
    Unbinder mUnbinder;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_start);

        // Init view binding.
        mUnbinder = ButterKnife.bind(this);

        // Toolbar.
        setSupportActionBar(mToolbar);
        if (getSupportActionBar() != null) {
            getSupportActionBar().setDisplayHomeAsUpEnabled(false);
            getSupportActionBar().setDisplayShowHomeEnabled(false);
        }

        // List menu.
        mStartMenu.setAdapter(onCreateSampleMenu());
        mStartMenu.setOnItemClickListener(onClickSampleMenuItem());
    }

    @Override
    public void showProgressBar() {
        ViewUtil
            .with(this)
            .setProgressBarCancelable(false)
            .showProgressBar(getString(R.string.loading));
    }

    @Override
    public void showProgressBar(String msg) {
        showProgressBar();
    }

    @Override
    public void hideProgressBar() {
        ViewUtil
            .with(this)
            .hideProgressBar();
    }

    @Override
    public void updateProgress(int progress) {
        showProgressBar();
    }

    ///////////////////////////////////////////////////////////////////////////
    // Protected / Private Methods ////////////////////////////////////////////

    @Override
    protected void onDestroy() {
        super.onDestroy();

        mUnbinder.unbind();
    }

    @SuppressWarnings({"unchecked"})
    protected SampleMenuAdapter onCreateSampleMenu() {
        return new SampleMenuAdapter(
            this,
            new SampleMenuItem[]{
                new SampleMenuItem(
                    "Detection using DLib",
                    "There're two steps of a complete face landmarks detection:\n" +
                    "(1) Detect face boundaries.\n" +
                    "(2) Given the face boundaries, align the landmarks to the " +
                    "faces.\n" +
                    "Two parts are done by using DLib.\n" +
                    "(about 10fps)",
                    new View.OnClickListener() {
                        @Override
                        public void onClick(View v) {
                            startActivity(
                                new Intent(StartActivity.this,
                                           SampleOfFacesAndLandmarksActivity1.class)
                                    .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP));
                        }
                    }),
            });
    }

    protected AdapterView.OnItemClickListener onClickSampleMenuItem() {
        return new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent,
                                    View view,
                                    int position,
                                    long id) {
                final SampleMenuItem item = (SampleMenuItem) parent.getAdapter()
                                                                   .getItem(position);
                item.onClickListener.onClick(view);
            }
        };
    }

    /**
     * Check the device to make sure it has the Google Play Services APK.
     */
    protected boolean checkPlayServices() {
        GoogleApiAvailability apiAvailability = GoogleApiAvailability.getInstance();
        int resultCode = apiAvailability.isGooglePlayServicesAvailable(this);
        if (resultCode != ConnectionResult.SUCCESS) {
            if (apiAvailability.isUserResolvableError(resultCode)) {
                apiAvailability.getErrorDialog(this, resultCode, 0)
                               .show();
            }
            return false;
        }
        return true;
    }

    ///////////////////////////////////////////////////////////////////////////
    // Clazz //////////////////////////////////////////////////////////////////
}
